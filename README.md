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

# Milestone 6 - Semaphores 

## Description
- Makes each node a critical zone
- The children wait for their turn
- the children are coloured in accordance with their status

## Compile
make milestone6

## Run
./sim input.txt

---
# Milestone 7 - Scheduling Algorithms

## Description
- Replaces random node entry with centralized scheduling algorithms.
- Implemented algorithms: FCFS (First Come First Serve) and SJF (Shortest Job First).
- [cite_start]The parent process manages the waiting queue and wakes up the pending processes based on the chosen scheduling algorithm[cite: 279].

## Compile
make milestone7

## Run
./sim -schd fcfs <file_name>
./sim -schd sjf <file_name>

## Comparison: FCFS vs SJF
In FCFS, travelers enter a node strictly based on their arrival time. While fair, this can cause significant delays if a traveler with a very long remaining path blocks a critical node. 
In SJF, the "Job Length" is determined by the number of remaining nodes a traveler has until reaching their destination. Changing to SJF significantly minimized the average waiting time for travelers who are closer to their finish line. By prioritizing shortest paths, the system clears travelers from the graph faster, resulting in optimized overall movement.

---
# Input Format (for milestones 1-3)

First line:
N M

Next M lines:
u v w

Last line:
src dst

# Input Format (For milestones 4-7)

First line:
N M

Next M lines:
u v w

Travelers:
k

Next k lines:
src dst
