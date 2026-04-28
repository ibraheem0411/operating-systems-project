#ifndef GUI_H
#define GUI_H

#include "Graph.h"
#include "raylib.h"

#define MAX_NODES 15

typedef struct
{
    Vector2 pos[MAX_NODES];
} Layout;

void computeLayout(Layout *layout, int N, Vector2 center);
void drawGraph(Graph *g, Layout *layout);

#endif