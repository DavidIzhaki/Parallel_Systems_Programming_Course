#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef int vertex;
typedef pthread_mutex_t mutex;

struct node {
    vertex v;
    struct node *next;
};

typedef struct node node;

struct Graph {
    unsigned int numVertices;
    node **adjacencyLists;
};

typedef struct Graph Graph;

// Function to create a node
node *createNode(vertex v) {
    node *newNode = (node *)malloc(sizeof(node));
    if (!newNode) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    newNode->v = v;
    newNode->next = NULL;
    return newNode;
}

// Function to create a graph with given vertices
Graph *createGraph(int vertices) {
    Graph *graph = (Graph *)malloc(sizeof(Graph));
    if (!graph) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    graph->numVertices = vertices;
    graph->adjacencyLists = (node **)malloc(vertices * sizeof(node *));
    if (!graph->adjacencyLists) {
        printf("Memory allocation failed\n");
        free(graph);
        exit(1);
    }

    for (int i = 0; i < vertices; i++) {
        graph->adjacencyLists[i] = NULL;
    }

    return graph;
}

// Function to add an edge to a directed graph
void addEdge(Graph *graph, vertex source, vertex destination) {
    // Add edge from source to destination
    node *newNode = createNode(destination);
    newNode->next = graph->adjacencyLists[source];
    graph->adjacencyLists[source] = newNode;
}

#endif
