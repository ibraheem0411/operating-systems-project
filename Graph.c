#include "Graph.h"

void printGraph(Graph *g)
{
    printf("Adjacency Matrix:\n");

    for (int i = 0; i < g->N; i++)
    {
        for (int j = 0; j < g->N; j++)
        {
            if (g->matrix[i][j] == INF)
                printf("INF ");
            else
                printf("%3d ", g->matrix[i][j]);
        }
        printf("\n");
    }
}

void freeGraph(Graph *g)
{
    if (!g)
        return;

    if (g->matrix)
    {
        for (int i = 0; i < g->N; i++)
            free(g->matrix[i]);

        free(g->matrix);
    }
    if (g->src)
        free(g->src);
    if (g->dst)
        free(g->dst);
    free(g);
}

Graph *parseGraph(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        perror("file could not open");
        exit(1);
    }

    Graph *g = malloc(sizeof(Graph));
    if (!g)
    {
        perror("malloc");
        fclose(fp);
        exit(1);
    }

    fscanf(fp, "%d %d", &g->N, &g->M);

    // allocate matrix
    g->matrix = malloc(g->N * sizeof(int *));
    for (int i = 0; i < g->N; i++)
        g->matrix[i] = malloc(g->N * sizeof(int));

    // init matrix
    for (int i = 0; i < g->N; i++)
        for (int j = 0; j < g->N; j++)
            g->matrix[i][j] = (i == j) ? 0 : INF;

    // read edges
    for (int i = 0; i < g->M; i++)
    {
        int u, v, w;
        fscanf(fp, "%d %d %d", &u, &v, &w);
        g->matrix[u][v] = w;
    }
    //scan number of travelers
    fscanf(fp, "%d", &g->travelers);
    g->src = malloc(g->travelers * sizeof(int));
    g->dst = malloc(g->travelers * sizeof(int));
    //scan source and destination for each traveler
    for (int i = 0; i < g->travelers; i++) {
        fscanf(fp, "%d %d", &g->src[i], &g->dst[i]);
    }

    fclose(fp);

    return g;
}

int *dijkstra(Graph *g, int *pathSize, int travelerIndex)
{
    int N = g->N;
    int src = g->src[travelerIndex];
    int dst = g->dst[travelerIndex];

    int dist[N];
    int visited[N];
    int parent[N];

    // init
    for (int i = 0; i < N; i++)
    {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[src] = 0;

    // Dijkstra main loop
    for (int i = 0; i < N - 1; i++)
    {
        int u = -1;
        int min = INF;

        for (int j = 0; j < N; j++)
        {
            if (!visited[j] && dist[j] < min)
            {
                min = dist[j];
                u = j;
            }
        }

        if (u == -1)
            break;

        visited[u] = 1;

        for (int v = 0; v < N; v++)
        {
            if (!visited[v] &&
                g->matrix[u][v] != INF &&
                dist[u] != INF &&
                dist[u] + g->matrix[u][v] < dist[v])
            {
                dist[v] = dist[u] + g->matrix[u][v];
                parent[v] = u;
            }
        }
    }

    // if no path
    if (parent[dst] == -1 && dst != src)
    {
        *pathSize = 0;
        return NULL;
    }

    // count path size
    int count = 0;
    int cur = dst;

    while (cur != -1)
    {
        count++;
        cur = parent[cur];
    }

    int *path = malloc(count * sizeof(int));
    *pathSize = count;

    // build path backwards
    cur = dst;

    for (int i = count - 1; i >= 0; i--)
    {
        path[i] = cur;
        cur = parent[cur];
    }

    return path;
}