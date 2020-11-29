/** @file simple_gain.c
 *
 * @brief Simple gain built by Eric Fontaine (CC0 2020) using jackaudio example program simple_client.c as starting point
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <math.h>

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

float dBmakeupgain = 0.0f; // user inputs gain in dB
float linearmakeupgain = 1.0f; // automatically calculated

float dBcompressorthreshold = -20.0f; // user input in dB
float linearcompressorthreshold = 0.1f; // automatically calculated

float compressorratio = 1.0f;

float maxamplitude = 0.0f;

int maxrows, maxcols;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 */
int
process (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *in, *out;
	
	in = jack_port_get_buffer (input_port, nframes);
	out = jack_port_get_buffer (output_port, nframes);

	int i;

	for (i = 0; i < nframes; i++) {
		if (in[i] < linearcompressorthreshold)
			out[i] = in[i] * linearmakeupgain;
		else
			out[i] = (in[i] + (in[i] - linearcompressorthreshold) / compressorratio) * linearmakeupgain; // apply gain

		if (fabs(in[i]) > maxamplitude)
			maxamplitude = fabs(in[i]);
	}

	return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
	exit (1);
}

int
main (int argc, char *argv[])
{
	const char **ports;
	const char *client_name = "simple";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	// ncurses setup
	initscr(); // ncurses init terminal
	getmaxyx(stdscr, maxrows, maxcols);
	cbreak; // only input one character at a time
	noecho(); // disable echoing of typed keyboard input
	keypad(stdscr, TRUE); // allow capture of special keystrokes, like arrow keys
	nodelay(stdscr, true); // make getch() non-blocking (to allow realtime output)

	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		exit (1);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	jack_on_shutdown (client, jack_shutdown, 0);

	/* display the current sample rate. 
	 */

	printf ("engine sample rate: %" PRIu32 "\n",
		jack_get_sample_rate (client));

	/* create two ports */

	input_port = jack_port_register (client, "input",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	output_port = jack_port_register (client, "output",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	if ((input_port == NULL) || (output_port == NULL)) {
		fprintf(stderr, "no more JACK ports available\n");
		exit (1);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		exit (1);
	}

	if (jack_connect (client, ports[0], jack_port_name (input_port))) {
		fprintf (stderr, "cannot connect input ports\n");
	}

	free (ports);
	
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical playback ports\n");
		exit (1);
	}

	if (jack_connect (client, jack_port_name (output_port), ports[0])) {
		fprintf (stderr, "cannot connect output ports\n");
	}

	free (ports);


	/* keep running until stopped by the user */
	while (true) {
		erase(); // clear screen

		mvprintw( 0, 0, "%1.4f ", maxamplitude);
		int nfullchars = maxamplitude  * (float) (maxcols - 6);

		int i;
		for ( i=0; i < nfullchars; i++)
			addch(ACS_CKBOARD);

		maxamplitude = 0.0f;

		mvprintw( 2, 0, "%+1.2f dB makeup gain (adjust with UP/DOWN)", dBmakeupgain);
		mvprintw( 3, 0, "%+1.2f dB compressor threshold (adjust with t/g)", dBcompressorthreshold);
		mvprintw( 4, 0, "%+1.2f compressor ratio (adjust with r/f)", compressorratio);

		int keystroke = getch();
		if (keystroke != ERR) {
		  switch (keystroke) {

			case KEY_UP:
			dBmakeupgain += 1.0f;
			break;

			case KEY_RIGHT:
			dBmakeupgain += 0.1f;
			break;

			case KEY_DOWN:
			dBmakeupgain -= 1.0f;
			break;

			case KEY_LEFT:
			dBmakeupgain -= 0.1f;
			break;

			case 't':
			dBcompressorthreshold += 1.0f;
			break;

			case 'T':
			dBcompressorthreshold += 0.1f;
			break;

			case 'g':
			dBcompressorthreshold -= 1.0f;
			break;

			case 'G':
			dBcompressorthreshold -= 0.1f;
			break;

			case 'r':
			compressorratio += 1.0f;
			break;

			case 'R':
			compressorratio += 0.1f;
			break;

			case 'f':
			compressorratio -= 1.0f;
			break;

			case 'F':
			compressorratio -= 0.1f;
			break;
		  }
		}
			
		if (dBmakeupgain > 0.0f)
			mvprintw( 5, 0, " warning: makeup gain exceeds 0 dB...be careful of clipping!\n");

		// calculate linear from 10 ^ (dB/20)
		linearmakeupgain = pow(10, dBmakeupgain/20.0f);
		linearcompressorthreshold = pow(10, dBcompressorthreshold/20.0f);

		clrtobot(); // clear rest of screen

		usleep (10000); // sleep in microseconds
	}

	/* this is never reached but if the program
	   had some other way to exit besides being killed,
	   they would be important to call.
	*/

	jack_client_close (client);
	exit (0);
}
