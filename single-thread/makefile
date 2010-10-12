zax : main.o
	gcc -O3 -lpthread -o zax main.o
zax.o : main.c zax-single.h
	gcc -O3 -lpthread -c main.c
clean :
	rm main.o
