#include "Graph.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }

    Graph *g = parseGraph(argv[1]);


    if (g->src == g->dst)
    {
        printf("%d\n0\n", g->src);
        freeGraph(g);
        return 0;
    }

    int size;
    int *path = dijkstra(g, &size);

    if (path == NULL)
    {
        printf("No path found\n");
    }
    else
    {
        // print path
        for (int i = 0; i < size; i++)
        {
            printf("%d", path[i]);
            if (i < size - 1)
                printf(" -> ");
        }
        printf("\n");

        // compute total weight
        int total = 0;
        for (int i = 0; i < size - 1; i++)
        {
            total += g->matrix[path[i]][path[i + 1]];
        }

        printf("%d\n", total);

        free(path);
    }

    freeGraph(g);
    return 0;
}