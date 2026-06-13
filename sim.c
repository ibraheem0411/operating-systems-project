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
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <time.h>

/* =====================================================
 * IPC CONTRACT (MILESTONE 6) - IMPLEMENTED BY PERSON 1
 * ===================================================== */
#define MAX_PATH_LEN 50

typedef struct {
    int traveler_id;
    int len;
    int path_nodes[MAX_PATH_LEN];
} PathMsg;

typedef enum {
    MSG_MOVING,    
    MSG_WAITING,   
    MSG_INSIDE,    
    MSG_LEFT,      
    MSG_DONE       
} MsgType;

typedef struct {
    pid_t pid;
    int traveler_id;
    int from_node;
    int to_node;
    MsgType type;
} StateMsg;

/* Combined traveler state enum */
typedef enum {
    MOVING,
    WAITING_FOR_NODE,
    INSIDE_NODE,
    FINISHED
} TravelerState;

/* Shared memory structure for inter-process communication */
typedef struct {
    TravelerState state;
    int currentNode;
    int nextNode;
} TravelerInfo;

/* =====================================================
 * OS-SAFE WRAPPERS (EINTR PROTECTION) - BY PERSON 1
 * ===================================================== */

// Prevents sem_wait from breaking if interrupted by SIGSTOP/SIGCONT
void safe_sem_wait(sem_t *sem) {
    while (sem_wait(sem) == -1) {
        if (errno == EINTR) continue; 
        else break;
    }
}

// Prevents sleep from exiting early if interrupted by OS signals
void safe_sleep(float seconds) {
    struct timespec req, rem;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (seconds - (time_t)seconds) * 1000000000L;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem; 
    }
}

/* ===================================================== */

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    Graph *g = parseGraph(argv[1]);
    if (!g) return 1;

    InitWindow(900, 700, "Milestone 6 - Integrated IPC with GUI");
    SetTargetFPS(60);

    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    /* =====================================================
     * SEMAPHORES (SHARED MEMORY) - IMPLEMENTED BY PERSON 1
     * ===================================================== */
    sem_t *node_sem = mmap(NULL, g->N * sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (node_sem == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    for (int i = 0; i < g->N; i++) {
        sem_init(&node_sem[i], 1, 1);
    }

    /* =====================================================
     * SHARED MEMORY FOR TRAVELER STATE (FROM VERSION 2)
     * ===================================================== */
    int shm_fd = shm_open("/traveler_state", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    size_t shmSize = g->travelers * sizeof(TravelerInfo);
    ftruncate(shm_fd, shmSize);

    TravelerInfo *shared = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    for (int i = 0; i < g->travelers; i++) {
        shared[i].state = MOVING;
        shared[i].currentNode = -1;
        shared[i].nextNode = -1;
    }

    /* =====================================================
     * STATE (PARENT ONLY)
     * ===================================================== */
    int **path = calloc(g->travelers, sizeof(int *));
    int *pathSize = calloc(g->travelers, sizeof(int));

    int *currentNode = calloc(g->travelers, sizeof(int));
    int *jumpCount = calloc(g->travelers, sizeof(int));
    float *edgeTimer = calloc(g->travelers, sizeof(float));
    
    MsgType *traveler_state = calloc(g->travelers, sizeof(MsgType));
    Vector2 *pos = calloc(g->travelers, sizeof(Vector2));

    bool isPlaying = true;
    Rectangle playBtn = {20, 50, 140, 40};

    /* =====================================================
     * PIPES (FROM VERSION 1)
     * ===================================================== */
    int pathPipe[2];
    int logPipe[2];

    pipe(pathPipe);
    pipe(logPipe);

    fcntl(logPipe[0], F_SETFL, O_NONBLOCK);

    pid_t *pid = malloc(g->travelers * sizeof(pid_t));

    /* =====================================================
     * FORK CHILDREN - FULLY IMPLEMENTED BY PERSON 1
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        pid[i] = fork();

        if (pid[i] == 0)
        {
            close(pathPipe[0]);
            close(logPipe[0]);

            int len = 0;
            int *myPath = dijkstra(g, &len, i);

            if (!myPath) {
                PathMsg pmsg;
                pmsg.traveler_id = i;
                pmsg.len = 0;
                write(pathPipe[1], &pmsg, sizeof(PathMsg));
                close(pathPipe[1]); close(logPipe[1]); _exit(0);
            }

            /* Update shared state */
            int first = myPath[0];
            shared[i].state = WAITING_FOR_NODE;
            shared[i].nextNode = first;

            /* Wait for node access */
            safe_sem_wait(&node_sem[first]);
            
            shared[i].state = INSIDE_NODE;
            shared[i].currentNode = first;

            /* Send path using both mechanisms for compatibility */
            PathMsg pmsg;
            pmsg.traveler_id = i;
            pmsg.len = len;
            memcpy(pmsg.path_nodes, myPath, len * sizeof(int));
            write(pathPipe[1], &pmsg, sizeof(PathMsg));

            StateMsg msg = {getpid(), i, myPath[0], myPath[0], MSG_MOVING};

            for (int j = 0; j < len - 1; j++)
            {
                int from = myPath[j];
                int to = myPath[j + 1];
                int weight = g->matrix[from][to];

                /* Update shared state */
                shared[i].state = MOVING;

                /* Send moving message */
                msg.type = MSG_MOVING; 
                msg.from_node = from; 
                msg.to_node = to;
                write(logPipe[1], &msg, sizeof(msg));
                
                /* Sleep for edge traversal */
                safe_sleep(weight * 0.3f); 

                /* Update shared state for waiting */
                shared[i].state = WAITING_FOR_NODE;
                shared[i].nextNode = to;

                msg.type = MSG_WAITING;
                write(logPipe[1], &msg, sizeof(msg));

                /* CRITICAL SECTION ENTRY */
                safe_sem_wait(&node_sem[to]); 

                /* Update shared state for inside node */
                shared[i].state = INSIDE_NODE;
                shared[i].currentNode = to;

                msg.type = MSG_INSIDE;
                write(logPipe[1], &msg, sizeof(msg));
                
                safe_sleep(1.0f); 

                /* CRITICAL SECTION EXIT */
                sem_post(&node_sem[to]); 

                msg.type = MSG_LEFT;
                write(logPipe[1], &msg, sizeof(msg));
            }

            /* Update shared state for finished */
            shared[i].state = FINISHED;

            msg.type = MSG_DONE; 
            msg.to_node = myPath[len - 1];
            write(logPipe[1], &msg, sizeof(msg));

            free(myPath); 
            close(pathPipe[1]); 
            close(logPipe[1]);
            _exit(0);
        }
    }

    close(pathPipe[1]);
    close(logPipe[1]);

    /* =====================================================
     * RECEIVE PATHS (COMBINED APPROACH)
     * ===================================================== */
    int received = 0;
    while (received < g->travelers)
    {
        PathMsg pmsg;
        if (read(pathPipe[0], &pmsg, sizeof(PathMsg)) == sizeof(PathMsg))
        {
            int t_id = pmsg.traveler_id;
            if (pmsg.len == 0) {
                pathSize[t_id] = 0; 
                received++; 
                continue;
            }

            pathSize[t_id] = pmsg.len;
            path[t_id] = malloc(pmsg.len * sizeof(int));
            memcpy(path[t_id], pmsg.path_nodes, pmsg.len * sizeof(int));

            pos[t_id] = layout.pos[path[t_id][0]];
            currentNode[t_id] = 0;
            jumpCount[t_id] = 0;
            edgeTimer[t_id] = 0;
            traveler_state[t_id] = MSG_MOVING;

            received++;
        }
        else usleep(1000);
    }

    /* =====================================================
     * MAIN LOOP - COMBINED FUNCTIONALITY
     * ===================================================== */
    while (!WindowShouldClose())
    {
        /* ---------------- LOGS & STATE UPDATES ---------------- */
        StateMsg msg;
        while (read(logPipe[0], &msg, sizeof(msg)) == sizeof(msg))
        {
            /* Update traveler_state array */
            traveler_state[msg.traveler_id] = msg.type;
            
            /* Print formatted logs */
            switch(msg.type) {
                case MSG_MOVING:
                    printf("[PID=%d] moving from node %d to node %d\n", 
                           msg.pid, msg.from_node, msg.to_node);
                    break;
                case MSG_WAITING:
                    printf("[PID=%d] waiting for node %d\n", 
                           msg.pid, msg.to_node);
                    break;
                case MSG_INSIDE:
                    printf("[PID=%d] inside node %d\n", 
                           msg.pid, msg.to_node);
                    break;
                case MSG_LEFT:
                    printf("[PID=%d] left node %d, now moving to node %d\n", 
                           msg.pid, msg.from_node, msg.to_node);
                    /* Update animation state when left is received */
                    if (currentNode[msg.traveler_id] < pathSize[msg.traveler_id] - 1) {
                        currentNode[msg.traveler_id]++;
                        jumpCount[msg.traveler_id] = 0;
                        edgeTimer[msg.traveler_id] = 0;
                    }
                    break;
                case MSG_DONE:
                    printf("[PID=%d] finished journey at node %d\n", 
                           msg.pid, msg.to_node);
                    break;
            }
        }

        float dt = GetFrameTime();

        /* ---------------- INPUT & OS PROCESS ALIGNMENT ---------------- */
        bool clicked = CheckCollisionPointRec(GetMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        if (clicked || IsKeyPressed(KEY_SPACE))
        {
            isPlaying = !isPlaying;
            /* Send real OS signals (SIGSTOP/SIGCONT) to child processes */
            for (int i = 0; i < g->travelers; i++) {
                if (pid[i] > 0) {
                    if (isPlaying) {
                        kill(pid[i], SIGCONT);
                    } else {
                        kill(pid[i], SIGSTOP);
                    }
                }
            }
        }

        /* ---------------- ANIMATION LOGIC ---------------- */
        if (isPlaying)
        {
            for (int i = 0; i < g->travelers; i++)
            {
                if (pathSize[i] < 2) continue;
                if (currentNode[i] >= pathSize[i] - 1) continue;

                /* Handle animation based on traveler_state */
                if (traveler_state[i] == MSG_INSIDE) {
                    /* Snap exactly to the target node's center */
                    int node_idx = path[i][currentNode[i] + 1];
                    pos[i] = layout.pos[node_idx];
                }
                else if (traveler_state[i] == MSG_WAITING) {
                    /* Offset position slightly from the target node so they don't overlap */
                    int target_node = path[i][currentNode[i] + 1];
                    Vector2 node_pos = layout.pos[target_node];
                    /* Add a small circular offset based on traveler ID */
                    float angle = (i * 2 * PI) / g->travelers;
                    pos[i].x = node_pos.x + cos(angle) * 15;
                    pos[i].y = node_pos.y + sin(angle) * 15;
                }
                else if (traveler_state[i] == MSG_MOVING) {
                    /* Interpolate position between nodes */
                    int from = path[i][currentNode[i]];
                    int to = path[i][currentNode[i] + 1];
                    
                    int weight = g->matrix[from][to];
                    if (weight <= 0) weight = 1;
                    
                    edgeTimer[i] += dt;
                    float t = fmin(1.0f, edgeTimer[i] / (weight * 0.3f));
                    
                    Vector2 a = layout.pos[from];
                    Vector2 b = layout.pos[to];
                    
                    pos[i].x = a.x + t * (b.x - a.x);
                    pos[i].y = a.y + t * (b.y - a.y);
                }
            }
        }

        /* ---------------- RENDER ---------------- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        drawGraph(g, &layout);

        DrawRectangleRec(playBtn, LIGHTGRAY);
        DrawRectangleLines(playBtn.x, playBtn.y, playBtn.width, playBtn.height, DARKGRAY);
        DrawText(isPlaying ? "STOP" : "PLAY", playBtn.x + 35, playBtn.y + 10, 20, BLACK);

        Color colors[] = {RED, BLUE, GREEN, PURPLE, ORANGE, YELLOW};

        for (int i = 0; i < g->travelers; i++)
        {
            if (pathSize[i] == 0) continue; 
            
            Color c;
            /* Determine color based on state from shared memory or local state */
            if (shared[i].state == WAITING_FOR_NODE || traveler_state[i] == MSG_WAITING) {
                c = YELLOW;
                /* Draw rectangle for waiting travelers */
                DrawRectangle(pos[i].x - 8, pos[i].y - 8, 16, 16, c);
                DrawRectangleLines(pos[i].x - 8, pos[i].y - 8, 16, 16, BLACK);
            } else {
                c = colors[i % 6];
                /* Draw circle for moving/inside travelers */
                DrawCircle(pos[i].x, pos[i].y, 10, c);
                DrawCircleLines(pos[i].x, pos[i].y, 10, BLACK);
            }
        }

        EndDrawing();
    }

    /* =====================================================
     * CLEANUP - COMBINED APPROACH
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        /* Send SIGCONT before SIGTERM to wake up stopped processes safely */
        if (pid[i] > 0) {
            kill(pid[i], SIGCONT);
            kill(pid[i], SIGTERM);
            waitpid(pid[i], NULL, 0);
        }
        if (pathSize[i] > 0) free(path[i]);
    }

    /* Clean up semaphores */
    for (int i = 0; i < g->N; i++) {
        sem_destroy(&node_sem[i]);
    }
    munmap(node_sem, g->N * sizeof(sem_t));

    /* Clean up shared memory */
    munmap(shared, shmSize);
    close(shm_fd);
    shm_unlink("/traveler_state");

    CloseWindow();

    /* Free all allocated memory */
    free(path); 
    free(pathSize); 
    free(currentNode);
    free(jumpCount); 
    free(edgeTimer); 
    free(traveler_state);
    free(pos); 
    free(pid); 
    freeGraph(g);

    return 0;
}
