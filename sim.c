#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "raylib.h"
#include "Graph.h"
#include "GUI.h"
#include "scheduler.h"

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

typedef struct {
    int traveler_id;
    int len;
} PathHeader;

typedef enum { REQ_WAIT, REQ_LEAVE } ReqType;

typedef struct {
    int traveler_id;
    int node_id;
    ReqType type;
} SchedulerReq;

typedef enum {
    MOVING,
    WAITING_FOR_NODE,
    INSIDE_NODE,
    FINISHED
} TravelerState;

typedef struct {
    TravelerState state;
    int currentNode;
    int nextNode;
    int remainingNodes;
} TravelerInfo;

int main(int argc, char **argv)
{
    SchedulerConfig config;

    if (parse_scheduler_args(argc, argv, &config) != 0) {
        return 1;
    }

    if (scheduler_init(config.type) != 0) {
        return 1;
    }

    Graph *g = parseGraph(config.input_file);
    if (!g) {
        fprintf(stderr, "Error: failed to load graph from '%s'.\n", config.input_file);
        scheduler_shutdown();
        return 1;
    }

    scheduler_configure(g->N);

    InitWindow(900, 700, "Milestone 7 - Scheduling Algorithms");
    SetTargetFPS(60);

    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    int **path = calloc((size_t)g->travelers, sizeof(int *));
    int *pathSize = calloc((size_t)g->travelers, sizeof(int));
    int *currentNode = calloc((size_t)g->travelers, sizeof(int));
    int *jumpCount = calloc((size_t)g->travelers, sizeof(int));
    float *edgeTimer = calloc((size_t)g->travelers, sizeof(float));
    Vector2 *pos = calloc((size_t)g->travelers, sizeof(Vector2));

    bool isPlaying = true;
    Rectangle playBtn = {20, 60, 140, 40};

    int *nodeOccupant = malloc((size_t)g->N * sizeof(int));
    for (int i = 0; i < g->N; i++) {
        nodeOccupant[i] = -1;
    }

    int pathPipe[2], logPipe[2], reqPipe[2];
    pipe(pathPipe);
    pipe(logPipe);
    pipe(reqPipe);

    fcntl(logPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(reqPipe[0], F_SETFL, O_NONBLOCK);

    pid_t *pid = malloc((size_t)g->travelers * sizeof(pid_t));

    int shm_fd = shm_open("/traveler_state", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        scheduler_shutdown();
        freeGraph(g);
        return 1;
    }

    size_t shmSize = (size_t)g->travelers * sizeof(TravelerInfo);
    ftruncate(shm_fd, (off_t)shmSize);

    TravelerInfo *shared = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        scheduler_shutdown();
        freeGraph(g);
        return 1;
    }

    for (int i = 0; i < g->travelers; i++) {
        shared[i].state = MOVING;
        shared[i].currentNode = -1;
        shared[i].nextNode = -1;
        shared[i].remainingNodes = 0;
    }

    sem_t **childSem = malloc((size_t)g->travelers * sizeof(sem_t *));
    for (int i = 0; i < g->travelers; i++) {
        char name[64];
        snprintf(name, sizeof(name), "/child_sem_%d", i);
        sem_unlink(name);
        childSem[i] = sem_open(name, O_CREAT, 0666, 0);
    }

    for (int i = 0; i < g->travelers; i++) {
        pid[i] = fork();

        if (pid[i] == 0) {
            close(pathPipe[0]);
            close(logPipe[0]);
            close(reqPipe[0]);

            int len = 0;
            int *myPath = dijkstra(g, &len, i);
            if (!myPath) {
                _exit(0);
            }

            PathHeader h = {i, len};
            write(pathPipe[1], &h, sizeof(h));
            write(pathPipe[1], myPath, (size_t)len * sizeof(int));

            for (int j = 0; j < len; j++) {
                int curr = myPath[j];
                shared[i].remainingNodes = len - j;

                shared[i].state = WAITING_FOR_NODE;
                shared[i].nextNode = curr;
                SchedulerReq req = {i, curr, REQ_WAIT};
                write(reqPipe[1], &req, sizeof(req));

                sem_wait(childSem[i]);

                shared[i].state = INSIDE_NODE;
                shared[i].currentNode = curr;

                if (j < len - 1) {
                    dprintf(logPipe[1], "[PID=%d] arrived at node %d | next node: %d\n",
                            getpid(), curr, myPath[j + 1]);
                } else {
                    dprintf(logPipe[1], "[PID=%d] arrived at node %d | DESTINATION\n",
                            getpid(), curr);
                }

                sleep(1);

                SchedulerReq leave = {i, curr, REQ_LEAVE};
                write(reqPipe[1], &leave, sizeof(leave));

                if (j < len - 1) {
                    shared[i].state = MOVING;
                    int weight = g->matrix[curr][myPath[j + 1]];
                    usleep((useconds_t)weight * 300000U);
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

    int received = 0;
    while (received < g->travelers) {
        PathHeader h;
        if (read(pathPipe[0], &h, sizeof(h)) <= 0) {
            usleep(1000);
            continue;
        }

        pathSize[h.traveler_id] = h.len;
        path[h.traveler_id] = malloc((size_t)h.len * sizeof(int));
        read(pathPipe[0], path[h.traveler_id], (size_t)h.len * sizeof(int));

        pos[h.traveler_id] = layout.pos[path[h.traveler_id][0]];
        currentNode[h.traveler_id] = 0;
        jumpCount[h.traveler_id] = 0;
        edgeTimer[h.traveler_id] = 0;

        received++;
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        bool clicked = CheckCollisionPointRec(GetMousePosition(), playBtn) &&
                       IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (clicked || IsKeyPressed(KEY_SPACE)) {
            isPlaying = !isPlaying;
            for (int i = 0; i < g->travelers; i++) {
                if (isPlaying) {
                    kill(pid[i], SIGCONT);
                } else {
                    kill(pid[i], SIGSTOP);
                }
            }
        }

        char buffer[512];
        int n;
        while ((n = read(logPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
        }

        if (isPlaying) {
            SchedulerReq req;
            while (read(reqPipe[0], &req, sizeof(req)) > 0) {
                int node = req.node_id;
                int tid = req.traveler_id;

                if (req.type == REQ_WAIT) {
                    if (nodeOccupant[node] == -1) {
                        nodeOccupant[node] = tid;
                        sem_post(childSem[tid]);
                    } else {
                        enqueue_waiting_process(node, tid, shared[tid].remainingNodes);
                    }
                } else if (req.type == REQ_LEAVE) {
                    nodeOccupant[node] = -1;

                    int next_tid = select_next_process(node);
                    if (next_tid != -1) {
                        nodeOccupant[node] = next_tid;
                        sem_post(childSem[next_tid]);
                    }
                }
            }
        }

        if (isPlaying) {
            for (int i = 0; i < g->travelers; i++) {
                if (pathSize[i] < 2 || shared[i].state == FINISHED) {
                    continue;
                }

                if (shared[i].state == WAITING_FOR_NODE || shared[i].state == INSIDE_NODE) {
                    int node = shared[i].nextNode;
                    if (node != -1) {
                        pos[i] = layout.pos[node];
                    }
                    jumpCount[i] = 0;
                    edgeTimer[i] = 0;
                } else if (shared[i].state == MOVING) {
                    int from = shared[i].currentNode;
                    int to = shared[i].nextNode;

                    if (from != -1 && to != -1) {
                        int weight = g->matrix[from][to];
                        if (weight <= 0) {
                            weight = 1;
                        }

                        edgeTimer[i] += dt;
                        if (edgeTimer[i] >= 0.3f) {
                            edgeTimer[i] -= 0.3f;
                            jumpCount[i]++;
                        }

                        float t = (float)jumpCount[i] / (float)weight;
                        if (t > 1.0f) {
                            t = 1.0f;
                        }

                        Vector2 a = layout.pos[from];
                        Vector2 b = layout.pos[to];
                        pos[i].x = a.x + t * (b.x - a.x);
                        pos[i].y = a.y + t * (b.y - a.y);
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        drawGraph(g, &layout);

        DrawText(TextFormat("Scheduler: %s", scheduler_get_active_name()),
                 20, 20, 20, RAYWHITE);

        DrawRectangleRec(playBtn, LIGHTGRAY);
        DrawRectangleLines((int)playBtn.x, (int)playBtn.y,
                           (int)playBtn.width, (int)playBtn.height, DARKGRAY);
        DrawText(isPlaying ? "PAUSE" : "PLAY", (int)playBtn.x + 35, (int)playBtn.y + 10, 20, BLACK);

        for (int i = 0; i < g->travelers; i++) {
            Color c;
            switch (shared[i].state) {
            case WAITING_FOR_NODE: c = YELLOW; break;
            case INSIDE_NODE:      c = GREEN;  break;
            case MOVING:           c = BLUE;   break;
            case FINISHED:         c = GRAY;   break;
            default:               c = RED;
            }
            DrawCircle((int)pos[i].x, (int)pos[i].y, 10, c);
            DrawCircleLines((int)pos[i].x, (int)pos[i].y, 10, BLACK);
        }

        EndDrawing();
    }

    for (int i = 0; i < g->travelers; i++) {
        kill(pid[i], SIGTERM);
        waitpid(pid[i], NULL, 0);
        free(path[i]);

        char name[64];
        snprintf(name, sizeof(name), "/child_sem_%d", i);
        sem_close(childSem[i]);
        sem_unlink(name);
    }

    munmap(shared, shmSize);
    close(shm_fd);
    shm_unlink("/traveler_state");

    CloseWindow();

    free(path);
    free(pathSize);
    free(currentNode);
    free(jumpCount);
    free(edgeTimer);
    free(pos);
    free(pid);
    free(nodeOccupant);
    free(childSem);
    freeGraph(g);
    scheduler_shutdown();

    return 0;
}
