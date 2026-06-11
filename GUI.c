#include "GUI.h"
#include <math.h>
#include <stdio.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// -------------------------
// Layout
// -------------------------
void computeLayout(Layout *layout, int N, Vector2 center)
{
    float radius = 230.0f;
    float angleStep = 2 * PI / N;

    for (int i = 0; i < N; i++)
    {
        float angle = i * angleStep;

        layout->pos[i].x = center.x + radius * cosf(angle);
        layout->pos[i].y = center.y + radius * sinf(angle);
    }
}

// -------------------------
// Arrow (clean + centered)
// -------------------------
static void drawArrow(Vector2 a, Vector2 b)
{
    Vector2 dir = {
        b.x - a.x,
        b.y - a.y};

    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len == 0)
        return;

    dir.x /= len;
    dir.y /= len;

    float r = 30.0f;

    Vector2 start = {a.x + dir.x * r, a.y + dir.y * r};
    Vector2 end = {b.x - dir.x * r, b.y - dir.y * r};

    // soft shadow (gives depth)
    DrawLineEx(start, end, 8.0f, Fade(BLACK, 0.25f));

    // main edge
    DrawLineEx(start, end, 4.0f, DARKGRAY);

    // arrow head
    Vector2 left = {
        end.x - dir.x * 14 - dir.y * 7,
        end.y - dir.y * 14 + dir.x * 7};

    Vector2 right = {
        end.x - dir.x * 14 + dir.y * 7,
        end.y - dir.y * 14 - dir.x * 7};

    DrawLineEx(end, left, 3.0f, DARKGRAY);
    DrawLineEx(end, right, 3.0f, DARKGRAY);
}

// -------------------------
// Graph drawing
// -------------------------
void drawGraph(Graph *g, Layout *layout)
{
    int N = g->N;

    // ========= EDGES =========
    for (int u = 0; u < N; u++)
    {
        for (int v = 0; v < N; v++)
        {
            if (g->matrix[u][v] != INF && u != v)
            {
                Vector2 a = layout->pos[u];
                Vector2 b = layout->pos[v];

                drawArrow(a, b);

                // weight label background
                Vector2 mid = {
                    (a.x + b.x) * 0.5f,
                    (a.y + b.y) * 0.5f};

                DrawCircle(mid.x, mid.y, 13, Fade(BLACK, 0.9f));

                DrawText(
                    TextFormat("%d", g->matrix[u][v]),
                    mid.x - 6,
                    mid.y - 10,
                    18,
                    MAROON);
            }
        }
    }

    // ========= NODES =========
    for (int i = 0; i < N; i++)
    {
        Vector2 p = layout->pos[i];

        // glow effect (outer soft ring)
        DrawCircle(p.x, p.y, 36, Fade(SKYBLUE, 0.15f));

        // border
        DrawCircle(p.x, p.y, 30, DARKBLUE);

        // inner fill
        DrawCircle(p.x, p.y, 26, SKYBLUE);

        // highlight core
        DrawCircle(p.x, p.y, 18, Fade(WHITE, 0.25f));

        // node id
        DrawText(
            TextFormat("%d", i),
            p.x - 6,
            p.y - 10,
            20,
            BLACK);
    }
}