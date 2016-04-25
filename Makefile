CFLAGS = #-DDEBUG

#CC = gcc
CC = ${CROSS_COMPILE}gcc $(CFLAGS)

TARGET = factory_burn
OBJS = main.o serial.o scp.o utiles.o

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

utiles.o : utiles.c
	$(CC) -c utiles.c -o utiles.o

scp.o : scp.c utiles.h
	$(CC) -c scp.c -o scp.o

serial.o : serial.c utiles.h serial.h
	$(CC) -c serial.c -o serial.o

main.o : main.c utiles.h
	$(CC) -c main.c -o main.o

clean :
	rm $(TARGET) *.o