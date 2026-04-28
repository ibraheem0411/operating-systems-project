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

# Input Format

First line:
N M

Next M lines:
u v w

Last line:
src dst
