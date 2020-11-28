simple_gain: simple_gain.c
	gcc -o simple_gain `pkg-config --cflags --libs jack` simple_gain.c
