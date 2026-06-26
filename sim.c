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

/* =====================================================
 * MESSAGE STRUCTURE - Child sends position updates only
 * ===================================================== */
typedef struct {
    pid_t pid;
    int traveler_id;
    int current_node;
    int next_node;      // -1 if destination or no path
    bool finished;      // true when reached destination
    bool no_path;       // true if no path exists
} PositionMessage;

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

    InitWindow(900, 700, "Milestone 5 - IPC Autonomous Children");
    SetTargetFPS(60);

    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    /* =====================================================
     * STATE (PARENT ONLY)
     * ===================================================== */
    Vector2 *pos = calloc(g->travelers, sizeof(Vector2));
    int *currentNode = calloc(g->travelers, sizeof(int));
    bool *finished = calloc(g->travelers, sizeof(bool));
    bool *noPath = calloc(g->travelers, sizeof(bool));

    // Initialize positions at source nodes
    for (int i = 0; i < g->travelers; i++) {
        pos[i] = layout.pos[g->src[i]];
        currentNode[i] = g->src[i];
    }

    bool isPlaying = true;
    Rectangle playBtn = {20, 50, 140, 40};

    /* =====================================================
     * PIPE - For position updates from children
     * ===================================================== */
    int msgPipe[2];
    if (pipe(msgPipe) < 0) {
        perror("pipe failed");
        return 1;
    }
    fcntl(msgPipe[0], F_SETFL, O_NONBLOCK);

    pid_t *pid = malloc(g->travelers * sizeof(pid_t));

    /* =====================================================
     * FORK CHILDREN
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        pid[i] = fork();

        if (pid[i] == 0)
        {
            // ---------- CHILD PROCESS ----------
            close(msgPipe[0]);  // close read end

            int len = 0;
            int *myPath = dijkstra(g, &len, i);

            // Check if no path exists (Disconnected Graph)
            if (!myPath || len == 0)
            {
                // Send NO PATH message
                PositionMessage msg = {
                    .pid = getpid(),
                    .traveler_id = i,
                    .current_node = g->src[i],
                    .next_node = -1,
                    .finished = true,
                    .no_path = true
                };
                write(msgPipe[1], &msg, sizeof(msg));
                
                close(msgPipe[1]);
                _exit(0);
            }

            // Travel along the path autonomously
            for (int step = 0; step < len; step++) {
                int current = myPath[step];
                int next = (step < len - 1) ? myPath[step + 1] : -1;

                // Send position update
                PositionMessage msg = {
                    .pid = getpid(),
                    .traveler_id = i,
                    .current_node = current,
                    .next_node = next,
                    .finished = (step == len - 1),
                    .no_path = false
                };
                write(msgPipe[1], &msg, sizeof(msg));

                // Wait before moving to next node based on edge weight
                if (step < len - 1) {
                    int from = myPath[step];
                    int to = myPath[step + 1];
                    int weight = g->matrix[from][to];
                    if (weight <= 0) weight = 1;
                    usleep(weight * 300000);  // 0.3 sec per weight unit
                }
            }

            free(myPath);
            close(msgPipe[1]);
            _exit(0);
        }
    }

    close(msgPipe[1]);  // parent closes write end

    /* =====================================================
     * MAIN LOOP - Receive messages and update GUI
     * ===================================================== */
    int finishedCount = 0;
    int noPathCount = 0;

    while (!WindowShouldClose())
    {
        /* ---------------- READ MESSAGES ---------------- */
        PositionMessage msg;
        int n;

        while ((n = read(msgPipe[0], &msg, sizeof(msg))) > 0)
        {
            int idx = msg.traveler_id;

            // Handle Special NO PATH Message
            if (msg.no_path) {
                if (!noPath[idx]) { // Prevent duplicate logs
                    noPath[idx] = true;
                    noPathCount++;
                    finished[idx] = true;
                    finishedCount++;
                    
                    // Print special error message to terminal as required
                    printf("[PID=%d] ERROR: No route found from node %d to destination %d! (Disconnected Graph)\n", 
                           msg.pid, g->src[idx], g->dst[idx]);
                    printf("[PID=%d] finished\n", msg.pid);
                    
                    pos[idx] = layout.pos[g->src[idx]];
                }
                continue;
            }

            // Update position for normal movement
            pos[idx] = layout.pos[msg.current_node];
            currentNode[idx] = msg.current_node;

            // Print normal log messages from parent process
            if (msg.next_node != -1) {
                printf("[PID=%d] arrived at node %d | next node: %d\n",
                       msg.pid, msg.current_node, msg.next_node);
            } else {
                printf("[PID=%d] arrived at node %d | DESTINATION\n",
                       msg.pid, msg.current_node);
            }

            // Handle normal finish
            if (msg.finished && !finished[idx]) {
                finished[idx] = true;
                finishedCount++;
                printf("[PID=%d] finished\n", msg.pid);
            }
        }

        /* ---------------- INPUT ---------------- */
        bool clicked =
            CheckCollisionPointRec(GetMousePosition(), playBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        if (clicked || IsKeyPressed(KEY_SPACE))
            isPlaying = !isPlaying;

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
            if (noPath[i]) {
                // Draw in gray with a red X for travelers with no path
                DrawCircle(pos[i].x, pos[i].y, 12, GRAY);
                DrawCircleLines(pos[i].x, pos[i].y, 12, DARKGRAY);
                DrawLine(pos[i].x - 6, pos[i].y - 6, pos[i].x + 6, pos[i].y + 6, RED);
                DrawLine(pos[i].x + 6, pos[i].y - 6, pos[i].x - 6, pos[i].y + 6, RED);
            } else {
                DrawCircle(pos[i].x, pos[i].y, 10, colors[i % 6]);
                DrawCircleLines(pos[i].x, pos[i].y, 10, colors[i % 6]);
            }
        }

        EndDrawing();

        // Exit loop when all travelers are done
        if (finishedCount == g->travelers) {
            printf("All travelers finished.\n");
            if (noPathCount > 0) {
                printf("%d traveler(s) found no path.\n", noPathCount);
            }
            break;
        }
    }

    /* =====================================================
     * CLEANUP
     * ===================================================== */
    for (int i = 0; i < g->travelers; i++)
    {
        kill(pid[i], SIGTERM);
        waitpid(pid[i], NULL, 0);
    }

    CloseWindow();

    free(pos);
    free(currentNode);
    free(finished);
    free(noPath);
    free(pid);
    freeGraph(g);

    return 0;
}
