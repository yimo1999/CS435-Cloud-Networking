all: ls

# vec: main.cpp monitor_neighbors.h
# 	g++ -pthread -o vec_router main.cpp monitor_neighbors.h

# vec: manager_send.c
# 	gcc -o manager manager_send.c

ls: main.cpp monitor_neighbors.h
	g++ -pthread -o ls_router main.cpp monitor_neighbors.h

.PHONY: clean
clean:
	rm *.o vec_router ls_router
