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

    InitWindow(900, 700, "Milestone 6 - (TODO: PERSON 2 GUI & Control)");
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
     * STATE
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
     * PIPES
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

            PathMsg pmsg;
            pmsg.traveler_id = i;
            
            if (!myPath) {
                pmsg.len = 0;
                write(pathPipe[1], &pmsg, sizeof(PathMsg));
                close(pathPipe[1]); close(logPipe[1]); _exit(0);
            }

            pmsg.len = len;
            memcpy(pmsg.path_nodes, myPath, len * sizeof(int));
            write(pathPipe[1], &pmsg, sizeof(PathMsg)); // Atomic write

            StateMsg msg = {getpid(), i, myPath[0], myPath[0], MSG_MOVING};

            for (int j = 0; j < len - 1; j++)
            {
                int from = myPath[j];
                int to = myPath[j + 1];
                int weight = g->matrix[from][to];

                msg.type = MSG_MOVING; msg.from_node = from; msg.to_node = to;
                write(logPipe[1], &msg, sizeof(msg));
                safe_sleep(weight * 0.3f); 

                msg.type = MSG_WAITING;
                write(logPipe[1], &msg, sizeof(msg));

                safe_sem_wait(&node_sem[to]); // CRITICAL SECTION ENTRY

                msg.type = MSG_INSIDE;
                write(logPipe[1], &msg, sizeof(msg));
                safe_sleep(1.0f); 

                sem_post(&node_sem[to]); // CRITICAL SECTION EXIT

                msg.type = MSG_LEFT;
                write(logPipe[1], &msg, sizeof(msg));
            }

            msg.type = MSG_DONE; msg.to_node = myPath[len - 1];
            write(logPipe[1], &msg, sizeof(msg));

            free(myPath); close(pathPipe[1]); close(logPipe[1]);
            _exit(0);
        }
    }

    close(pathPipe[1]);
    close(logPipe[1]);

    /* =====================================================
     * RECEIVE PATHS
     * ===================================================== */
    int received = 0;
    while (received < g->travelers)
    {
        PathMsg pmsg;
        if (read(pathPipe[0], &pmsg, sizeof(PathMsg)) == sizeof(PathMsg))
        {
            int t_id = pmsg.traveler_id;
            if (pmsg.len == 0) {
                pathSize[t_id] = 0; received++; continue;
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
     * MAIN LOOP - TODO FOR PERSON 2
     * ===================================================== */
    while (!WindowShouldClose())
    {
        /* ---------------- LOGS & STATE UPDATES ---------------- */
        StateMsg msg;
        while (read(logPipe[0], &msg, sizeof(msg)) == sizeof(msg))
        {
            // TODO: PERSON 2 - Update traveler_state array here
            // TODO: PERSON 2 - Print formatted logs (e.g. "[PID=X] arrived at node Y | next node: Z")
            // TODO: PERSON 2 - Update currentNode, jumpCount, and edgeTimer when type == MSG_LEFT
        }

        float dt = GetFrameTime();

        /* ---------------- INPUT & OS PROCESS ALIGNMENT ---------------- */
        bool clicked = CheckCollisionPointRec(GetMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        if (clicked || IsKeyPressed(KEY_SPACE))
        {
            isPlaying = !isPlaying;
            // TODO: PERSON 2 - Send real OS signals (SIGSTOP/SIGCONT) to child processes (pid[i]) here!
        }

        /* ---------------- ANIMATION LOGIC ---------------- */
        if (isPlaying)
        {
            // TODO: PERSON 2 - Write animation logic based on traveler_state[i]
            // If MSG_MOVING -> interpolate position between nodes based on edge weight.
            // If MSG_WAITING -> offset position slightly from the target node so they don't overlap.
            // If MSG_INSIDE -> snap exactly to the target node's center.
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
            Color c = colors[i % 6];
            
            // TODO: PERSON 2 - If traveler_state[i] == MSG_WAITING, draw a GRAY RECTANGLE instead of a circle!
            DrawCircle(pos[i].x, pos[i].y, 10, c);
            DrawCircleLines(pos[i].x, pos[i].y, 10, BLACK);
        }

        EndDrawing();
    }

    /* =====================================================
     * CLEANUP - IMPLEMENTED BY PERSON 1
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        // TODO: PERSON 2 - Add a SIGCONT signal here before SIGTERM to wake up stopped processes safely!
        kill(pid[i], SIGTERM);
        waitpid(pid[i], NULL, 0);
        if (pathSize[i] > 0) free(path[i]);
    }

    for (int i = 0; i < g->N; i++) {
        sem_destroy(&node_sem[i]);
    }
    munmap(node_sem, g->N * sizeof(sem_t));

    CloseWindow();

    free(path); free(pathSize); free(currentNode);
    free(jumpCount); free(edgeTimer); free(traveler_state);
    free(pos); free(pid); freeGraph(g);

    return 0;
}
