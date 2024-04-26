CC = gcc
CFLAGS = -Wall

all: parent cargoPlane

parent: parent.c
	@$(CC) $(CFLAGS) -o parent parent.c

cargoPlane: cargoPlane.c
	@$(CC) $(CFLAGS) -o cargoPlane cargoPlane.c

run: parent cargoPlane
	@echo "compiling and running the program..."
	@./parent configuration.txt

clean:
	@rm -f parent cargoPlane *.o
	@sudo rm /run/shm/MEM1 2> /dev/null