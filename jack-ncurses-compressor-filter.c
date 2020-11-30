/** @file jack-ncurses-compressor-filter.c
 *
 * @brief Simple compressor & filter with makeup gain by Eric Fontaine (CC0 2020).
 * used jackaudio example program simple_client.c as starting point
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

float makeupGain_dB = 0.0f; // user inputs gain in dB
float makeupGain = 1.0f; // automatically calculated

float compressorThreshold_dB = 0.0f; // user input in dB
float compressorThreshold = 1.0f; // user input in dB

float compressorRatio = 1.0f;

float maxAmplitudeInput = 0.0f;
float maxAmplitudeOutput = 0.0f;

int maxRows, maxCols;

static inline float linear_from_dB(float dB) {
	return powf(10.0f, 0.05f * dB);
}

static inline float dB_from_linear(float linear) {
	return 20.0f * log10f(linear);
}

float compress(float absoluteValueInput)
{
	if (absoluteValueInput > compressorThreshold) {
		float absoluteValueInput_dB = dB_from_linear(absoluteValueInput);
		return linear_from_dB(compressorThreshold_dB + (absoluteValueInput_dB - compressorThreshold_dB) / compressorRatio);
	}
	else
		return absoluteValueInput;
}

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

		bool isNegative = in[i] < 0.0f;

		float absoluteValue = fabs(in[i]);

		if (absoluteValue > maxAmplitudeInput)
			maxAmplitudeInput = absoluteValue;

		float absoluteCompressed = compress(absoluteValue);
		float absoluteCompressedGained = absoluteCompressed * makeupGain;

		if (absoluteCompressedGained > 1.0f)
			absoluteCompressedGained = 1.0f;

		if (absoluteCompressedGained > maxAmplitudeOutput)
			maxAmplitudeOutput = absoluteCompressedGained;

		out[i] = isNegative ? -absoluteCompressedGained : absoluteCompressedGained;
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


void printbar( float amplitude, int columnsavailable)
{
		int nfullchars = (columnsavailable > 0) ? amplitude * (float) columnsavailable : 0;

		int i;
		for ( i=0; i < nfullchars; i++)
			addch(ACS_CKBOARD);
}

int
main (int argc, char *argv[])
{
	const char **ports;
	const char *client_name = "compressor-filter";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	// ncurses setup
	initscr(); // ncurses init terminal
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

		getmaxyx(stdscr, maxRows, maxCols);
		int barCols = maxCols > 24 ? maxCols - 24 : 0;

		mvprintw( 0, 0, "input amplitude:  %1.4f ", maxAmplitudeInput);
		printbar( maxAmplitudeInput, barCols);
		maxAmplitudeInput = 0.0f;
		if (compressorThreshold < 1.0f) {
			int col = 24 + ((float) compressorThreshold) * barCols;
			mvprintw(0, col, "|");
		}

		mvprintw( 1, 0, "output amplitude: %1.4f ", maxAmplitudeOutput);
		printbar( maxAmplitudeOutput, barCols);
		maxAmplitudeOutput = 0.0f;
		float compressorThresholdTimesMakeupGain = compressorThreshold * makeupGain;
		if (compressorThresholdTimesMakeupGain < 1.0f) {
			int col = 24 + ((float) compressorThresholdTimesMakeupGain) * barCols;
			mvprintw(1, col, "|");
		}

		mvprintw( 2, 0, "%+1.2f dB (%1.3f) makeup gain (adjust with (SHIFT) +/-)", makeupGain_dB, makeupGain);
		mvprintw( 3, 0, "%+1.2f dB (%1.3f) compressor threshold (adjust with (SHIFT) t/g)", compressorThreshold_dB, compressorThreshold);
		mvprintw( 4, 0, "%+1.2f compressor ratio (adjust with (SHIFT) r/f)", compressorRatio);

		int keystroke = getch();
		if (keystroke != ERR) {
		  switch (keystroke) {

			case '=':
			makeupGain_dB += 1.0f;
			break;

			case '+':
			makeupGain_dB += 0.1f;
			break;

			case '-':
			makeupGain_dB -= 1.0f;
			break;

			case '_':
			makeupGain_dB -= 0.1f;
			break;

			case 't':
			compressorThreshold_dB += 1.0f;
			break;

			case 'T':
			compressorThreshold_dB += 0.1f;
			break;

			case 'g':
			compressorThreshold_dB -= 1.0f;
			break;

			case 'G':
			compressorThreshold_dB -= 0.1f;
			break;

			case 'r':
			compressorRatio += 1.0f;
			break;

			case 'R':
			compressorRatio += 0.1f;
			break;

			case 'f':
			compressorRatio -= 1.0f;
			break;

			case 'F':
			compressorRatio -= 0.1f;
			break;
		  }
		}
			
		if (makeupGain_dB > 0.0f)
			mvprintw( 5, 0, " warning: makeup gain exceeds 0 dB...be careful of clipping!\n");

		if (compressorRatio < 1.0f)
			compressorRatio = 1.0f;

		// calculate linear from 10 ^ (dB/10)
		makeupGain = linear_from_dB(makeupGain_dB);
		compressorThreshold = linear_from_dB(compressorThreshold_dB);

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
