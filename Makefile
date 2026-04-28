CC = gcc
CFLAGS = -Wall -Wextra -std=c99

TARGET = dijkstra

SRC = dijkstra.c Graph.c

# build milestone 1
milestone1:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# run convenience (optional, not required but useful)
run:
	./$(TARGET) input.txt

clean:
	rm -f $(TARGET)