#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "raylib.h"
#include "Graph.h"
#include "GUI.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

/* ===================================================== */

typedef struct
{
    int traveler_id;
    int len;
} PathHeader;

/* ======= SCHEDULER IPC DATA ======= */
typedef enum { REQ_WAIT, REQ_LEAVE } ReqType;

typedef struct {
    int traveler_id;
    int node_id;
    ReqType type;
} SchedulerReq;

/* ===================================================== */

typedef enum
{
    MOVING,
    WAITING_FOR_NODE,
    INSIDE_NODE,
    FINISHED
} TravelerState;

/* ===================================================== */

typedef struct
{
    TravelerState state;
    int currentNode;
    int nextNode;
    int remainingNodes; // Used for SJF Scheduling
} TravelerInfo;

/* ===================================================== */

int main(int argc, char **argv)
{
    char schedAlgo[10] = "fcfs"; // Default algorithm
    char *inputFile = NULL;

    // Milestone 7 args check
    if (argc == 4&& strcmp(argv[1], "-schd") == 0)
    {
        strcpy(schedAlgo, argv[2]);
        inputFile = argv[3];
    }
    else if (argc == 2)
    {
        inputFile = argv[1];
    }
    else
    {
        printf("Usage for M7: %s <-schd> <fcfs|sjf> <input_file>\n", argv[0]);
        printf("Usage for M2-M6: %s <input_file>\n", argv[0]);
        return 1;
    }

    Graph *g = parseGraph(inputFile);
    if (!g) return 1;

    InitWindow(900, 700, "Milestone 7 - Scheduling Algorithms");
    SetTargetFPS(60);

    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    /* =====================================================
     * STATE (PARENT ONLY)
     * ===================================================== */
    int **path = calloc(g->travelers, sizeof(int *));
    int *pathSize = calloc(g->travelers, sizeof(int));

    int *currentNode = calloc(g->travelers, sizeof(int));
    int *jumpCount = calloc(g->travelers, sizeof(int));
    float *edgeTimer = calloc(g->travelers, sizeof(float));

    Vector2 *pos = calloc(g->travelers, sizeof(Vector2));

    bool isPlaying = true;
    Rectangle playBtn = {20, 60, 140, 40};

    /* ======= SCHEDULER QUEUES ======= */
    int *nodeOccupant = malloc(g->N * sizeof(int));
    for(int i=0; i<g->N; i++) nodeOccupant[i] = -1;

    bool *isWaiting = calloc(g->travelers, sizeof(bool));
    int *waitingForNode = calloc(g->travelers, sizeof(int));
    double *arrivalTime = calloc(g->travelers, sizeof(double));

    /* =====================================================
     * PIPES
     * ===================================================== */
    int pathPipe[2], logPipe[2], reqPipe[2];
    pipe(pathPipe);
    pipe(logPipe);
    pipe(reqPipe);

    fcntl(logPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(reqPipe[0], F_SETFL, O_NONBLOCK); // Parent reads requests without blocking

    pid_t *pid = malloc(g->travelers * sizeof(pid_t));

    /* =====================================================
     * SHARED MEMORY
     * ===================================================== */
    int shm_fd = shm_open("/traveler_state", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) exit(1);

    size_t shmSize = g->travelers * sizeof(TravelerInfo);
    ftruncate(shm_fd, shmSize);

    TravelerInfo *shared = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) exit(1);

    for (int i = 0; i < g->travelers; i++)
    {
        shared[i].state = MOVING;
        shared[i].currentNode = -1;
        shared[i].nextNode = -1;
        shared[i].remainingNodes = 0;
    }

    /* =====================================================
     * INDIVIDUAL SEMAPHORES (For precise scheduling)
     * ===================================================== */
    sem_t **childSem = malloc(g->travelers * sizeof(sem_t *));
    for (int i = 0; i < g->travelers; i++)
    {
        char name[64];
        sprintf(name, "/child_sem_%d", i);
        sem_unlink(name);
        // Init to 0 so child waits until parent schedules it
        childSem[i] = sem_open(name, O_CREAT, 0666, 0); 
    }

    /* =====================================================
     * FORK CHILDREN
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        pid[i] = fork();

        if (pid[i] == 0)
        {
            close(pathPipe[0]);
            close(logPipe[0]);
            close(reqPipe[0]);

            int len = 0;
            int *myPath = dijkstra(g, &len, i);
            if (!myPath) exit(0);

            // Send path to parent
            PathHeader h = {i, len};
            write(pathPipe[1], &h, sizeof(h));
            write(pathPipe[1], myPath, len * sizeof(int));

            // Traverse Path
            for (int j = 0; j < len; j++)
            {
                int curr = myPath[j];
                shared[i].remainingNodes = len - j; // Important for SJF!

                // 1. Request Entry
                shared[i].state = WAITING_FOR_NODE;
                shared[i].nextNode = curr;
                SchedulerReq req = {i, curr, REQ_WAIT};
                write(reqPipe[1], &req, sizeof(req));

                // 2. Wait for Parent Scheduler to pick me
                sem_wait(childSem[i]);

                // 3. I am inside!
                shared[i].state = INSIDE_NODE;
                shared[i].currentNode = curr;

                if (j < len - 1)
                    dprintf(logPipe[1], "[PID=%d] arrived at node %d | next node: %d\n", getpid(), curr, myPath[j+1]);
                else
                    dprintf(logPipe[1], "[PID=%d] arrived at node %d | DESTINATION\n", getpid(), curr);

                sleep(1); // Stay inside node for 1 second

                // 4. Leave Node
                SchedulerReq leave = {i, curr, REQ_LEAVE};
                write(reqPipe[1], &leave, sizeof(leave));

                // 5. Move on Edge
                if (j < len - 1)
                {
                    shared[i].state = MOVING;
                    int weight = g->matrix[curr][myPath[j+1]];
                    usleep(weight * 300000); // 300ms per edge weight unit
                }
            }

            shared[i].state = FINISHED;
            dprintf(logPipe[1], "[PID=%d] finished\n", getpid());

            free(myPath);
            close(pathPipe[1]);
            close(logPipe[1]);
            close(reqPipe[1]);
            _exit(0);
        }
    }

    close(pathPipe[1]);
    close(logPipe[1]);
    close(reqPipe[1]);

    /* =====================================================
     * RECEIVE PATHS
     * ===================================================== */
    int received = 0;
    while (received < g->travelers)
    {
        PathHeader h;
        if (read(pathPipe[0], &h, sizeof(h)) <= 0)
        {
            usleep(1000);
            continue;
        }

        pathSize[h.traveler_id] = h.len;
        path[h.traveler_id] = malloc(h.len * sizeof(int));
        read(pathPipe[0], path[h.traveler_id], h.len * sizeof(int));

        pos[h.traveler_id] = layout.pos[path[h.traveler_id][0]];
        currentNode[h.traveler_id] = 0;
        jumpCount[h.traveler_id] = 0;
        edgeTimer[h.traveler_id] = 0;

        received++;
    }

    /* =====================================================
     * MAIN LOOP (SCHEDULER & ANIMATION)
     * ===================================================== */
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        /* ---------------- INPUT (Play/Pause) ---------------- */
        bool clicked = CheckCollisionPointRec(GetMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (clicked || IsKeyPressed(KEY_SPACE))
        {
            isPlaying = !isPlaying;
            // Send signal to pause/resume children POSIX processes 
            for(int i=0; i<g->travelers; i++) {
                if (isPlaying) kill(pid[i], SIGCONT);
                else kill(pid[i], SIGSTOP);
            }
        }

        /* ---------------- LOGS ---------------- */
        char buffer[512];
        int n;
        while ((n = read(logPipe[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[n] = 0;
            printf("%s", buffer);
        }

        /* ---------------- SCHEDULER LOGIC ---------------- */
        if (isPlaying)
        {
            SchedulerReq req;
            while (read(reqPipe[0], &req, sizeof(req)) > 0)
            {
                int node = req.node_id;
                int tid = req.traveler_id;

                if (req.type == REQ_WAIT)
                {
                    if (nodeOccupant[node] == -1) // Node is free
                    {
                        nodeOccupant[node] = tid;
                        sem_post(childSem[tid]); // Wake up child immediately
                    }
                    else // Node is occupied, put in queue
                    {
                        isWaiting[tid] = true;
                        waitingForNode[tid] = node;
                        arrivalTime[tid] = GetTime(); // For FCFS
                    }
                }
                else if (req.type == REQ_LEAVE)
                {
                    nodeOccupant[node] = -1;

                    // Scheduling: Pick next traveler from queue [cite: 279]
                    int best_id = -1;

                    if (strcmp(schedAlgo, "sjf") == 0)
                    {
                        int min_job = 999999;
                        double min_time = 1e9;
                        for (int i = 0; i < g->travelers; i++)
                        {
                            if (isWaiting[i] && waitingForNode[i] == node)
                            {
                                int rem = shared[i].remainingNodes; // Job size = remaining nodes
                                // Pick shortest job, fallback to FCFS if equal
                                if (rem < min_job || (rem == min_job && arrivalTime[i] < min_time))
                                {
                                    min_job = rem;
                                    min_time = arrivalTime[i];
                                    best_id = i;
                                }
                            }
                        }
                    }
                    else // fcfs default
                    {
                        double min_time = 1e9;
                        for (int i = 0; i < g->travelers; i++)
                        {
                            if (isWaiting[i] && waitingForNode[i] == node)
                            {
                                if (arrivalTime[i] < min_time)
                                {
                                    min_time = arrivalTime[i];
                                    best_id = i;
                                }
                            }
                        }
                    }

                    // Wake up the scheduled child
                    if (best_id != -1)
                    {
                        nodeOccupant[node] = best_id;
                        isWaiting[best_id] = false;
                        sem_post(childSem[best_id]);
                    }
                }
            }
        }

        /* ---------------- ANIMATION ---------------- */
        if (isPlaying)
        {
            for (int i = 0; i < g->travelers; i++)
            {
                if (pathSize[i] < 2 || shared[i].state == FINISHED) continue;

                if (shared[i].state == WAITING_FOR_NODE || shared[i].state == INSIDE_NODE)
                {
                    // Snap exactly to node position to avoid visual desync
                    int node = shared[i].nextNode;
                    if(node != -1) pos[i] = layout.pos[node];
                    jumpCount[i] = 0; 
                    edgeTimer[i] = 0;
                }
                else if (shared[i].state == MOVING)
                {
                    int from = shared[i].currentNode;
                    int to = shared[i].nextNode;
                    
                    if (from != -1 && to != -1) {
                        int weight = g->matrix[from][to];
                        if (weight <= 0) weight = 1;

                        edgeTimer[i] += dt;
                        if (edgeTimer[i] >= 0.3f)
                        {
                            edgeTimer[i] -= 0.3f;
                            jumpCount[i]++;
                        }

                        float t = (float)jumpCount[i] / weight;
                        if(t > 1.0f) t = 1.0f;

                        Vector2 a = layout.pos[from];
                        Vector2 b = layout.pos[to];
                        pos[i].x = a.x + t * (b.x - a.x);
                        pos[i].y = a.y + t * (b.y - a.y);
                    }
                }
            }
        }

        /* ---------------- RENDER ---------------- */
        BeginDrawing();
        ClearBackground(BLACK);

        drawGraph(g, &layout);

        // UI 
        DrawText(TextFormat("Scheduler: %s", (strcmp(schedAlgo, "sjf") == 0) ? "SJF (Shortest Job First)" : "FCFS (First Come First Serve)"), 20, 20, 20, RAYWHITE);

        DrawRectangleRec(playBtn, LIGHTGRAY);
        DrawRectangleLines(playBtn.x, playBtn.y, playBtn.width, playBtn.height, DARKGRAY);
        DrawText(isPlaying ? "PAUSE" : "PLAY", playBtn.x + 35, playBtn.y + 10, 20, BLACK);

        for (int i = 0; i < g->travelers; i++)
        {
            Color c;
            switch (shared[i].state)
            {
                case WAITING_FOR_NODE: c = YELLOW; break;
                case INSIDE_NODE: c = GREEN; break;
                case MOVING: c = BLUE; break;
                case FINISHED: c = GRAY; break;
                default: c = RED;
            }
            DrawCircle(pos[i].x, pos[i].y, 10, c);
            DrawCircleLines(pos[i].x, pos[i].y, 10, BLACK);
        }

        EndDrawing();
    }

    /* =====================================================
     * CLEANUP
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        kill(pid[i], SIGTERM);
        waitpid(pid[i], NULL, 0);
        free(path[i]);
        
        char name[64];
        sprintf(name, "/child_sem_%d", i);
        sem_close(childSem[i]);
        sem_unlink(name);
    }

    munmap(shared, shmSize);
    close(shm_fd);
    shm_unlink("/traveler_state");

    CloseWindow();

    free(path); free(pathSize); free(currentNode);
    free(jumpCount); free(edgeTimer); free(pos);
    free(pid); free(nodeOccupant); free(isWaiting);
    free(waitingForNode); free(arrivalTime); free(childSem);
    freeGraph(g);

    return 0;
}
