CC = gcc
CFLAGS = -Wall

all: parent cargoPlane committee monitor

parent: parent.c
	@$(CC) $(CFLAGS) -o parent parent.c

cargoPlane: cargoPlane.c
	@$(CC) $(CFLAGS) -o cargoPlane cargoPlane.c

monitor: monitor.c
	@$(CC) $(CFLAGS) -o monitor monitor.c

committee: collectorsCommittee.c
	@$(CC) $(CFLAGS) -o collectorsCommittee collectorsCommittee.c

run: parent cargoPlane committee monitor 
	@echo "compiling and running the program..."
	@./parent configuration.txt

clean:
	@rm -f parent cargoPlane monitor collectorsCommittee *.o
	@sudo rm /run/shm/MEM1 2> /dev/null
	@sudo rm /run/shm/MEM2 2> /dev/null
	@sudo rm /run/shm/MEM3 2> /dev/null
	@sudo rm /run/shm/sem.SEM1 2> /dev/null
	@sudo rm /run/shm/sem.SEM2 2> /dev/null
	@sudo rm /run/shm/sem.SEM3 2> /dev/null