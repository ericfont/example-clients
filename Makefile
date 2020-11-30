jack-ncurses-compressor-filter: jack-ncurses-compressor-filter.c
	gcc -o jack-ncurses-compressor-filter `pkg-config --cflags --libs jack` -lncurses -lm jack-ncurses-compressor-filter.c

clean:
	rm -f jack-ncurses-compressor-filter
