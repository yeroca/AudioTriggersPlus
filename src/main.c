/*===============================================================================================
 PlaySound Example
 Copyright (c), Firelight Technologies Pty, Ltd 2004-2010.

 This example shows how to simply load and play multiple sounds.  This is about the simplest
 use of FMOD.
 This makes FMOD decode the into memory when it loads.  If the sounds are big and possibly take
 up a lot of ram, then it would be better to use the FMOD_CREATESTREAM flag so that it is 
 streamed in realtime as it plays.
 ===============================================================================================*/
#include "../inc/fmod.h"
#include "../inc/fmod_errors.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define DEBUG 0
#if DEBUG
#define debugmsg(args...) fprintf(stderr, args)
#else
#define debugmsg(args...)
#endif

char *log_file_names[] = {
		"/home/corey/log1",
		"/home/corey/log2"
};

struct event_buffer_entry {
	int sound_id;
};

/* NUM_EVENTS must be a power of 2 */
#define NUM_EVENTS 32
struct event_buffer {
	struct event_buffer_entry entry[NUM_EVENTS];
	int cur, next;
	pthread_mutex_t lock;
	pthread_cond_t events_available;
};

struct event_buffer events;

struct log_file_info {
	pthread_t thread;
	FILE *file;
};

struct log_file_info *lfi;

static inline int num_log_files(void)
{
	return sizeof(log_file_names) / sizeof(char *);
}

void ERRCHECK(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}
}


const char *case_insensitive_strstr(const char *search_in, const char *search_for)
{
	if (*search_for == '\0') {
		return search_in;
	}
	for (; *search_in; ++search_in) {
		if (toupper(*search_in) == toupper(*search_for)) {
			/*
			 * Matched starting char -- loop through remaining chars.
			 */
			const char *_in, *_for;
			for (_in = search_in, _for = search_for; *_in && *_for; ++_in, ++_for) {
				if (toupper(*_in) != toupper(*_for)) {
					break;
				}
			}
			if (!*_for) /* matched all of 'search_for' till the null terminator */
			{
				return search_in; /* return the start of the match */
			}
		}
	}
	return NULL;
}

static void enqueue_sound(int sound_id)
{
	pthread_mutex_lock(&events.lock);
	events.next = (events.next + 1) % NUM_EVENTS;
	debugmsg("cur = %d, next = %d\n", events.cur, events.next);
	if (events.next == events.cur) {
		fprintf(stderr, "event queue overflow!\n");
		exit(1);
	}
	events.entry[events.next].sound_id = sound_id;
	pthread_cond_signal(&events.events_available);
	pthread_mutex_unlock(&events.lock);
}


struct trigger {
	char *pattern;
	bool stop_search_when_match;
	int sound_to_play;
};

struct trigger triggers[] = { { "play sound 1", false, 1 }, { "play sound 2", false, 2 }, { "play sound 3", false, 3 } };

void *logwatcher(void *arg) {
	intptr_t log_file_num = (intptr_t)arg;
	char tail_cmd[1024];
	int i;
	size_t buffer_size = 1024;
	char *buffer;

	buffer = malloc(buffer_size);

	sprintf(tail_cmd, "/usr/bin/tail --lines=0 --follow --sleep-interval=0.05 %s", log_file_names[log_file_num]);
	debugmsg("attempting to open pipe for command: %s\n", tail_cmd);
	lfi[log_file_num].file = popen(tail_cmd, "r");
	if (lfi[log_file_num].file == NULL) {
		fprintf(stderr, "popen return NULL for \"%s\"\n", tail_cmd);
	}
	while (1) {
		getline(&buffer, &buffer_size, lfi[log_file_num].file);
		debugmsg("got line: %s", buffer);
		for (i = 0; i < sizeof(triggers) / sizeof(struct trigger); i++) {
			if (case_insensitive_strstr(buffer, triggers[i].pattern)) {
				switch (triggers[i].sound_to_play) {
				case 1:
					enqueue_sound(1);
					break;
				case 2:
					enqueue_sound(2);
					break;
				case 3:
					enqueue_sound(3);
					break;
				}
				if (triggers[i].stop_search_when_match)
					break;
			}
		}
	}
}


int main(int argc, char *argv[]) {
	FMOD_SYSTEM *system;
	FMOD_SOUND *sound1, *sound2, *sound3;
	FMOD_CHANNEL *channel = 0;
	FMOD_RESULT result;
	unsigned int version;
	int i, max_fd = -1, ret, log_num;
	fd_set log_fd_set;
	char *line;
	char buffer[1024];

	FD_ZERO(&log_fd_set);

	lfi = malloc(sizeof(struct log_file_info) * num_log_files());
	ret = pthread_cond_init(&events.events_available, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events cond object\n");
	}
	ret = pthread_mutex_init(&events.lock, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events lock object\n");
	}


	for (i = 0; i < num_log_files(); i++) {
		ret = pthread_create(&lfi[i].thread, NULL, logwatcher, (void *)(intptr_t)i);
		if (ret < 0) {
			fprintf(stderr, "Unable to create logwatcher pthread for log file %d\n", i);
			exit(1);
		}
	}

	/*
	 Create a System object and initialize.
	 */
	result = FMOD_System_Create(&system);
	ERRCHECK(result);

	result = FMOD_System_GetVersion(system, &version);
	ERRCHECK(result);

	if (version < FMOD_VERSION) {
		printf("Error!  You are using an old version of FMOD %08x.  This program requires %08x\n",
			version, FMOD_VERSION);
		return 0;
	}

	result = FMOD_System_Init(system, 32, FMOD_INIT_NORMAL, NULL);
	ERRCHECK(result);

	result = FMOD_System_CreateSound(system, "../media/drumloop.wav",
			FMOD_SOFTWARE, 0, &sound1);
	ERRCHECK(result);
	result = FMOD_Sound_SetMode(sound1, FMOD_LOOP_OFF);
	ERRCHECK(result);
	{
		float freq, vol, pan;
		int prio;
		result = FMOD_Sound_GetDefaults(sound1, &freq, &vol, &pan, &prio);
		ERRCHECK(result);
		result = FMOD_Sound_SetDefaults(sound1, freq, vol, -1.0, prio);
		ERRCHECK(result);
	}

	result = FMOD_System_CreateSound(system, "../media/drumloop.wav",
			FMOD_SOFTWARE, 0, &sound2);
	ERRCHECK(result);
	result = FMOD_Sound_SetMode(sound2, FMOD_LOOP_OFF);
	ERRCHECK(result);
	{
		float freq, vol, pan;
		int prio;
		result = FMOD_Sound_GetDefaults(sound2, &freq, &vol, &pan, &prio);
		ERRCHECK(result);
		result = FMOD_Sound_SetDefaults(sound2, freq, vol, 1.0, prio);
		ERRCHECK(result);

	}

	result = FMOD_System_CreateSound(system, "../media/drumloop.wav",
			FMOD_SOFTWARE, 0, &sound3);
	ERRCHECK(result);
	result = FMOD_Sound_SetMode(sound3, FMOD_LOOP_OFF);
	ERRCHECK(result);
	{
		float freq, vol, pan;
		int prio;
		result = FMOD_Sound_GetDefaults(sound3, &freq, &vol, &pan, &prio);
		ERRCHECK(result);
		result = FMOD_Sound_SetDefaults(sound3, freq, vol, 0.0, prio);
		ERRCHECK(result);

	}

	/*
	 Main loop.
	 */
	pthread_mutex_lock(&events.lock);
	while (1) {
		while (events.cur == events.next) {
			debugmsg("event queue is empty\n");
			pthread_cond_wait(&events.events_available, &events.lock);
		}
		events.cur = (events.cur + 1) % NUM_EVENTS;
		switch (events.entry[events.cur].sound_id) {
		case 1:
			result = FMOD_System_PlaySound(system,
			                FMOD_CHANNEL_FREE, sound1, 0, &channel);
			ERRCHECK(result);
			break;
		case 2:
			result = FMOD_System_PlaySound(system,
			                FMOD_CHANNEL_FREE, sound2, 0, &channel);
			ERRCHECK(result);
			break;
		case 3:
			result = FMOD_System_PlaySound(system,
			                FMOD_CHANNEL_FREE, sound3, 0, &channel);
			ERRCHECK(result);
			break;
		}
	}

	/*
	 Shut down
	 */
	result = FMOD_Sound_Release(sound1);
	ERRCHECK(result);
	result = FMOD_Sound_Release(sound2);
	ERRCHECK(result);
	result = FMOD_Sound_Release(sound3);
	ERRCHECK(result);
	result = FMOD_System_Close(system);
	ERRCHECK(result);
	result = FMOD_System_Release(system);
	ERRCHECK(result);

	return 0;
}

