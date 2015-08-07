app_burn : main.o serial.o scp.o utiles.o
	gcc -o app_burn main.o serial.o scp.o utiles.o

utiles.o : utiles.c
	gcc -c utiles.c -o utiles.o

scp.o : scp.c utiles.h
	gcc -c scp.c -o scp.o

serial.o : serial.c utiles.h
	gcc -c serial.c -o serial.o

main.o : main.c utiles.h
	gcc -c main.c -o main.o

clean :
	rm app_burn *.o