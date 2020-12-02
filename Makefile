jack-curses-lowpass-compressor-gain: jack-curses-lowpass-compressor-gain.c
	gcc jack-curses-lowpass-compressor-gain.c -o jack-curses-lowpass-compressor-gain -lpdcurses -lm -ljack

clean:
	rm -f jack-curses-lowpass-compressor-gain \
	      jack-curses-lowpass-compressor-gain.exe
