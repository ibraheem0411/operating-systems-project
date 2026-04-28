#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>

#define INF 10000000000

typedef struct
{
    int N;
    int M;
    int **matrix;
    int src;
    int dst;
} Graph;

// functions
Graph *parseGraph(const char *path);
void freeGraph(Graph *g);
void printGraph(Graph *g);
int *dijkstra(Graph *g, int *pathSize);

#endif