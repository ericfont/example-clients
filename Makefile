jack-ncurses-compressor-filter: jack-ncurses-compressor-filter.c
	gcc jack-ncurses-compressor-filter.c -o jack-ncurses-compressor-filter -lncurses -lm -ljack

clean:
	rm -f jack-ncurses-compressor-filter \
	      jack-ncurses-compressor-filter.exe
