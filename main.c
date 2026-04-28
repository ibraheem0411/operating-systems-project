#include "Graph.h"

int main()
{
    Graph *g = parseGraph("input");

    int size;
    int *path = dijkstra(g, &size);

    if (path == NULL)
    {
        printf("No path exists\n");
    }
    else
    {
        printf("Shortest path: ");
        for (int i = 0; i < size; i++)
        {
            printf("%d", path[i]);
            if (i < size - 1)
                printf(" -> ");
        }
        printf("\n");

        free(path);
    }

    freeGraph(g);
    return 0;
}