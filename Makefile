CC = gcc
CFLAGS = -Wall

all: parent cargoPlane committee monitor splitter

parent: parent.c
	@$(CC) $(CFLAGS) -o parent parent.c

cargoPlane: cargoPlane.c
	@$(CC) $(CFLAGS) -o cargoPlane cargoPlane.c

monitor: monitor.c
	@$(CC) $(CFLAGS) -o monitor monitor.c

committee: collectorsCommittee.c
	@$(CC) $(CFLAGS) -o collectors collectorsCommittee.c

splitter: splitter.c
	@$(CC) $(CFLAGS) -o splitter splitter.c	

run: parent cargoPlane committee monitor splitter
	@echo "compiling and running the program..."
	@./parent configuration.txt

clean:
	@rm -f parent cargoPlane monitor collectors splitter *.o
	# @sudo rm /run/shm/MEM1 2> /dev/null
	# @sudo rm /run/shm/MEM2 2> /dev/null
	# @sudo rm /run/shm/MEM3 2> /dev/null
	# @sudo rm /run/shm/MEM5 2> /dev/null
	# @sudo rm /run/shm/sem.SEM1 2> /dev/null
	# @sudo rm /run/shm/sem.SEM2 2> /dev/null
	# @sudo rm /run/shm/sem.SEM3 2> /dev/null
	# @sudo rm /run/shm/sem.SEM5 2> /dev/null