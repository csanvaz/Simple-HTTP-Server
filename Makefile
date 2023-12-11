all: hw4.c
	gcc -fsanitize=address -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Werror -std=c17 -Wpedantic -O0 -g hw4.c -o hw4

clean:
	rm -rf *.o hw4 d
