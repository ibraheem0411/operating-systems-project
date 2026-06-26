CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Raylib flags (Linux)
RAYLIB = -lraylib -lm -lpthread -ldl -lrt -lX11

# executables
D1 = dijkstra
D2 = sim

# source files
CORE = Graph.c
M2   = GUI.c sim.c

# ----------------------
# Milestone 1
# ----------------------
milestone1:
	$(CC) $(CFLAGS) dijkstra.c $(CORE) -o $(D1)

# ----------------------
# Milestone 2-6
# ----------------------
milestone2:
	$(CC) $(CFLAGS) $(M2) $(CORE) -o $(D2) $(RAYLIB)

milestone4:
	$(CC) $(CFLAGS) $(M2) $(CORE) -o $(D2) $(RAYLIB)

milestone5:
	$(CC) $(CFLAGS) $(M2) $(CORE) -o $(D2) $(RAYLIB)

milestone6:
	$(CC) $(CFLAGS) $(M2) $(CORE) -o $(D2) $(RAYLIB)

# ----------------------
# Milestone 7 (Scheduler)
# ----------------------
milestone7:
	$(CC) $(CFLAGS) $(M2) $(CORE) -o $(D2) $(RAYLIB)

.PHONY: raylib

raylib:
	sudo apt update
	sudo apt install -y build-essential cmake git \
		libx11-dev libxrandr-dev libxi-dev \
		libgl1-mesa-dev libxcursor-dev libxinerama-dev
	rm -rf /tmp/raylib
	git clone https://github.com/raysan5/raylib.git /tmp/raylib
	mkdir -p /tmp/raylib/build
	cd /tmp/raylib/build && cmake .. && make && sudo make install
	sudo ldconfig


# ----------------------
# clean
# ----------------------
clean:
	rm -f $(D1) $(D2)
