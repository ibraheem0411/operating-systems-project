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

/* ===================================================== */

typedef struct
{
    int traveler_id;
    int len;
} PathHeader;

/* ===================================================== */

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    Graph *g = parseGraph(argv[1]);
    if (!g)
        return 1;

    InitWindow(900, 700, "Milestone 5 - FIXED IPC");
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
    bool *waiting = calloc(g->travelers, sizeof(bool));

    Vector2 *pos = calloc(g->travelers, sizeof(Vector2));

    bool isPlaying = true;
    Rectangle playBtn = {20, 50, 140, 40};

    /* =====================================================
     * PIPE (PATH + LOGS)
     * ===================================================== */
    int pathPipe[2];
    int logPipe[2];

    pipe(pathPipe);
    pipe(logPipe);

    fcntl(logPipe[0], F_SETFL, O_NONBLOCK);

    pid_t *pid = malloc(g->travelers * sizeof(pid_t));

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

            int len = 0;
            int *myPath = dijkstra(g, &len, i);

            if (!myPath)
                exit(0);

            /* send path */
            PathHeader h = {i, len};
            write(pathPipe[1], &h, sizeof(h));
            write(pathPipe[1], myPath, len * sizeof(int));

            /* =====================================================
             * CHILD LOGGING (REQUIRED FORMAT)
             * ===================================================== */
            for (int j = 0; j < len - 1; j++)
            {
                int from = myPath[j];
                int to = myPath[j + 1];
                int weight = g->matrix[from][to];
                usleep(weight * 300000);

                dprintf(logPipe[1],
                        "[PID=%d] arrived at node %d | next node: %d\n",
                        getpid(),
                        from,
                        to);
            }

            dprintf(logPipe[1],
                    "[PID=%d] arrived at node %d | DESTINATION\n",
                    getpid(),
                    myPath[len - 1]);

            dprintf(logPipe[1],
                    "[PID=%d] finished\n",
                    getpid());

            free(myPath);
            close(pathPipe[1]);
            close(logPipe[1]);
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
        waiting[h.traveler_id] = true;

        received++;
    }

    /* =====================================================
     * MAIN LOOP (ANIMATION)
     * ===================================================== */
    while (!WindowShouldClose())
    {
        /* ---------------- LOGS ---------------- */
        char buffer[512];
        int n;

        while ((n = read(logPipe[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[n] = 0;
            printf("%s", buffer);
        }

        float dt = GetFrameTime();

        /* ---------------- INPUT ---------------- */
        bool clicked =
            CheckCollisionPointRec(GetMousePosition(), playBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        if (clicked || IsKeyPressed(KEY_SPACE))
            isPlaying = !isPlaying;

        /* ---------------- ANIMATION ---------------- */
        if (isPlaying)
        {
            for (int i = 0; i < g->travelers; i++)
            {
                if (pathSize[i] < 2)
                    continue;

                if (currentNode[i] >= pathSize[i] - 1)
                    continue;

                /* =========================
                 * NODE WAIT (1 sec)
                 * ========================= */
                if (waiting[i])
                {
                    edgeTimer[i] += dt;

                    if (edgeTimer[i] >= 1.0f)
                    {
                        waiting[i] = false;
                        edgeTimer[i] = 0;
                    }

                    continue;
                }

                int from = path[i][currentNode[i]];
                int to = path[i][currentNode[i] + 1];

                int weight = g->matrix[from][to];
                if (weight <= 0)
                    weight = 1;

                /* =========================
                 * EDGE JUMP LOGIC
                 * ========================= */
                edgeTimer[i] += dt;

                if (edgeTimer[i] >= 0.3f)
                {
                    edgeTimer[i] = 0;
                    jumpCount[i]++;

                    float t = (float)jumpCount[i] / weight;

                    Vector2 a = layout.pos[from];
                    Vector2 b = layout.pos[to];

                    pos[i].x = a.x + t * (b.x - a.x);
                    pos[i].y = a.y + t * (b.y - a.y);

                    if (jumpCount[i] >= weight)
                    {
                        currentNode[i]++;
                        jumpCount[i] = 0;
                        pos[i] = layout.pos[to];

                        waiting[i] = true;
                        edgeTimer[i] = 0;
                    }
                }
            }
        }

        /* ---------------- RENDER ---------------- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        drawGraph(g, &layout);

        DrawRectangleRec(playBtn, LIGHTGRAY);
        DrawRectangleLines(playBtn.x, playBtn.y, playBtn.width, playBtn.height, DARKGRAY);

        DrawText(isPlaying ? "STOP" : "PLAY",
                 playBtn.x + 35, playBtn.y + 10, 20, BLACK);

        Color colors[] = {RED, BLUE, GREEN, PURPLE, ORANGE, YELLOW};

        for (int i = 0; i < g->travelers; i++)
        {
            DrawCircle(pos[i].x, pos[i].y, 10, colors[i % 6]);
            DrawCircleLines(pos[i].x, pos[i].y, 10, colors[i % 6]);
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
    }

    CloseWindow();

    free(path);
    free(pathSize);
    free(currentNode);
    free(jumpCount);
    free(edgeTimer);
    free(waiting);
    free(pos);
    free(pid);
    freeGraph(g);

    return 0;
}