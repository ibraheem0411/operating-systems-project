# Operating Systems Project - Graph Simulation

## Overview
This project implements a graph system with Dijkstra's algorithm and a GUI visualization using raylib.

---

# Milestone 1 - Dijkstra Algorithm

## Description
- Reads a directed weighted graph from a file
- Builds adjacency matrix
- Computes shortest path using Dijkstra
- Outputs path and total weight

## Compile
make milestone1

## Run
./dijkstra input.txt

---

# Milestone 2 - Graph Visualization (raylib)

## Description
- Visualizes graph using raylib
- Nodes displayed as circles
- Directed edges shown with arrows
- Weights displayed on edges

## Compile
make milestone2

## Run
./sim input.txt

---

# Milestone 3 - Animation (raylib)

## Description
Animates the movement between the nodes 

## Compile
make milestone2

## Run
./sim input.txt

---

# Milestone 4 - Parent and Children (raylib)

## Description
creates multiple multiple children that move in their own paths during the animation

## Compile
make milestone4

## Run
./sim input.txt

---

# Milestone 5 - IPC (raylib)

## Description
- makes the children compute their paths independently 
- communication between child and parent using PIPE
- logs the status of the children with messages

## Compile
make milestone5

## Run
./sim input.txt

---

# Input Format (for milestones 1-3)

First line:
N M

Next M lines:
u v w

Last line:
src dst

# Input Format (For milestones 4-5)

First line:
N M

Next M lines:
u v w

Travelers:
k

Next k lines:
src dst
