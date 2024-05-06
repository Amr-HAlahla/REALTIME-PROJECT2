CC = gcc
CFLAGS = -Wall

all: parent cargoPlane committee monitor splitter distributers families occupation

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

distributers: distributers.c
	@$(CC) $(CFLAGS) -o distributers distributers.c

families: families.c
	@$(CC) $(CFLAGS) -o families families.c

occupation: occupationForces.c
	@$(CC) $(CFLAGS) -o occupation occupationForces.c	

run: parent cargoPlane committee monitor splitter distributers families occupation
	@echo "compiling and running the program..."
	@./parent configuration.txt

clean:
	@rm -f *.o parent cargoPlane monitor collectors splitter distributers families occupation *.log