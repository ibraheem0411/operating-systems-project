#define _POSIX_C_SOURCE 200809L

#include "raylib.h"
#include "Graph.h"
#include "GUI.h"

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h> // for O_NONBLOCK

int main(int argc, char **argv)
{
    // -------------------------
    // Check input file
    // -------------------------
    if (argc != 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // -------------------------
    // Load graph
    // -------------------------
    Graph *g = parseGraph(argv[1]);
    if (!g)
    {
        printf("Failed to load graph\n");
        return 1;
    }

    // -------------------------
    // Init window
    // -------------------------
    InitWindow(900, 700, "Graph Visualization - Milestone 3");
    SetTargetFPS(60);

    // -------------------------
    // Compute layout once (static graph)
    // -------------------------
    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});
    bool *hasPath = malloc(g->travelers * sizeof(bool));
    // -------------------------
    // Build shortest path once
    // -------------------------
    int pathSize = 0;
    int **path = malloc(g->travelers * sizeof(int *));
    for (int i = 0; i < g->travelers; i++)
    {
        path[i] = dijkstra(g, &pathSize, i);
        if (path)
        {
            printf("Traveler %d: Path from %d to %d: ", i, g->src[i], g->dst[i]);
            for (int j = 0; j < pathSize; j++)
            {
                printf("%d ", path[i][j]);
            }
            hasPath[i] = true;
        }
        else
        {
            printf("Traveler %d: No path found from %d to %d", i, g->src[i], g->dst[i]);
            hasPath[i] = false;
        }
        printf("\n");
    }

    bool isAnimating = true;

    Rectangle playStopBtn = {20.0f, 50.0f, 120.0f, 40.0f};
    int *currentNodeIndex = calloc(g->travelers, sizeof(int));
    int *currentJump = calloc(g->travelers, sizeof(int));
    float *timer = calloc(g->travelers, sizeof(float));
    bool *waitingAtNode = calloc(g->travelers, sizeof(bool));
    Vector2 *agentPos = malloc(g->travelers * sizeof(Vector2));
    int *pathSizes = malloc(g->travelers * sizeof(int));
    for (int i = 0; i < g->travelers; i++)
    {
        currentNodeIndex[i] = 0;
        currentJump[i] = 0;
        timer[i] = 0.0f;
        waitingAtNode[i] = false;
        if (hasPath[i])
        {
            agentPos[i] = layout.pos[path[i][0]];
        }
        else
        {
            agentPos[i] = (Vector2){0, 0};
        }
        pathSizes[i] = pathSize;
    }

    // pipe for communication between the parent and the children
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        exit(1);
    }

    // non blocking pipe
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    // forks
    pid_t *pid = malloc(g->travelers * sizeof(pid_t));

    for (int i = 0; i < g->travelers; i++)
    {
        pid[i] = fork();
        if (pid[i] < 0)
        {
            perror("fork failed");
            exit(1);
        }
        if (pid[i] == 0)
        {
            int traveler_id = i;
            printf("[%d] started\n", getpid());

            close(pipefd[0]); // closing read end for child

            // child computes its own path
            int pathLen = 0;
            int *myPath = dijkstra(g, &pathLen, traveler_id);

            if (!myPath)
            {
                exit(0);
            }

            for (int j = 0; j < pathLen - 1; j++)
            {
                sleep(1); // placeholder for the speed time

                char msg[128];
                snprintf(msg, sizeof(msg),
                         "[PID=%d] arrived at node %d | next node: %d\n",
                         getpid(),
                         myPath[j],
                         myPath[j + 1]);

                write(pipefd[1], msg, strlen(msg));
            }

            // final msg
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "[PID=%d] finished\n",
                     getpid());

            free(myPath);
            write(pipefd[1], msg, strlen(msg));
            close(pipefd[1]);
            exit(0);
        }
    }

    close(pipefd[1]); // parent doesn't write

    // -------------------------
    // Main render loop
    // -------------------------
    while (!WindowShouldClose())
    {
        char buffer[512];

        int n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[n] = '\0';
            printf("%s", buffer);
        }

        float dt = GetFrameTime();
        bool clickedButton = CheckCollisionPointRec(GetMousePosition(), playStopBtn) &&
                             IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Toggle animation state (Play/Stop)
        if ((clickedButton || IsKeyPressed(KEY_SPACE)) && hasPath)
        {
            isAnimating = !isAnimating;
        }
        for (int i = 0; i < g->travelers; i++)
        {
            // Run movement logic only while animation is active
            if (isAnimating && hasPath[i] && currentNodeIndex[i] < pathSizes[i] - 1)
            {
                timer[i] += dt;
                if (waitingAtNode[i])
                {
                    if (timer[i] >= 1.0f)
                    { // Wait 1 second at each node
                        waitingAtNode[i] = false;
                        timer[i] = 0.0f;
                    }
                }
                else
                {
                    int from = path[i][currentNodeIndex[i]];
                    int to = path[i][currentNodeIndex[i] + 1];
                    int weight = g->matrix[from][to];
                    if (timer[i] >= 0.3f)
                    {
                        currentJump[i]++;
                        timer[i] = 0.0f;
                    }
                    if (currentJump[i] >= weight)
                    {
                        currentNodeIndex[i]++;
                        currentJump[i] = 0;
                        agentPos[i] = layout.pos[path[i][currentNodeIndex[i]]];
                        if (currentNodeIndex[i] < pathSizes[i] - 1)
                        {
                            waitingAtNode[i] = true;
                        }
                    }
                    else
                    {
                        float t = (float)currentJump[i] / weight;
                        Vector2 fromPos = layout.pos[from];
                        Vector2 toPos = layout.pos[to];
                        agentPos[i].x = fromPos.x + t * (toPos.x - fromPos.x);
                        agentPos[i].y = fromPos.y + t * (toPos.y - fromPos.y);
                    }
                }
            }
        }
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Title
        DrawText("Graph Visualization (Milestone 3)", 10, 10, 20, DARKGRAY);

        // Draw graph
        drawGraph(g, &layout);

        // Draw play/stop button
        DrawRectangleRec(playStopBtn, LIGHTGRAY);
        DrawRectangleLines((int)playStopBtn.x, (int)playStopBtn.y, (int)playStopBtn.width, (int)playStopBtn.height, DARKGRAY);
        DrawText(isAnimating ? "Stop" : "Play",
                 (int)playStopBtn.x + 35,
                 (int)playStopBtn.y + 11,
                 20,
                 BLACK);
        bool allPathsCompleted = true;
        if (!hasPath)
        {
            DrawText("No path found between source and destination.", 20, 105, 20, RED);
        }
        else
        {
            // Animated entity
            Color colors[] = {RED, BLUE, GREEN, PURPLE, ORANGE, YELLOW};
            for (int i = 0; i < g->travelers; i++)
            {
                DrawCircle((int)agentPos[i].x, (int)agentPos[i].y, 10, colors[i % 6]);
                DrawCircleLines((int)agentPos[i].x, (int)agentPos[i].y, 10, colors[i % 6]);
            }
            for (int i = 0; i < g->travelers; i++)
            {
                if (currentNodeIndex[i] < pathSizes[i] - 1)
                {
                    allPathsCompleted = false;
                    break;
                }
            }
            if (allPathsCompleted)
            {
                DrawText("All travelers have reached their destination!", 20, 105, 20, GREEN);
                isAnimating = false;
            }
        }

        EndDrawing();
    }

    for (int i = 0; i < g->travelers; i++)
    {
        kill(pid[i], SIGTERM);
        waitpid(pid[i], NULL, 0);
    }
    // -------------------------
    // Cleanup
    // -------------------------
    CloseWindow();
    free(path);
    freeGraph(g);

    return 0;
}