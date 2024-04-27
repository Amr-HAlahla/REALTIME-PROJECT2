CC = gcc
CFLAGS = -Wall

all: parent cargoPlane monitor

parent: parent.c
	@$(CC) $(CFLAGS) -o parent parent.c

cargoPlane: cargoPlane.c
	@$(CC) $(CFLAGS) -o cargoPlane cargoPlane.c

monitor: monitor.c
	@$(CC) $(CFLAGS) -o monitor monitor.c

run: parent cargoPlane monitor
	@echo "compiling and running the program..."
	@./parent configuration.txt

clean:
	@rm -f parent cargoPlane monitor *.o
	@sudo rm /run/shm/MEM1 2> /dev/null
	@sudo rm /run/shm/MEM2 2> /dev/null