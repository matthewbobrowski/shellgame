BIN=shellgame

shellgame: shellgame.c
	gcc -Wall -Werror -O3 -pthread -o shellgame shellgame.c

clean:
	rm $(BIN)
