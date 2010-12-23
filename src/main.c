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
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#define DEBUG 1
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

static inline int get_num_log_files(void)
{
	return sizeof(log_file_names) / sizeof(char *);
}

void ERRCHECK(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		fprintf(stderr, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}
}


const char *case_insensitive_strstr(const char *search_in, const char *search_for)
{
	if (*search_for == '\0') {
		return search_in;
	}
	for (; *search_in; ++search_in) {
		if (toupper((int)*search_in) == toupper((int)*search_for)) {
			/*
			 * Matched starting char -- loop through remaining chars.
			 */
			const char *_in, *_for;
			for (_in = search_in, _for = search_for; *_in && *_for; ++_in, ++_for) {
				if (toupper((int)*_in) != toupper((int)*_for)) {
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
	char *name;
	char *pattern;
	bool stop_search_when_match;
	char *sound_to_play;
	int sound_to_play_id;

};

struct trigger triggers[] = {
		{ .name = "sound0", .pattern = "play sound 0", .stop_search_when_match = false, .sound_to_play = "drum_left" },
		{ .name = "sound1", .pattern = "play sound 1", .stop_search_when_match = false, .sound_to_play = "drum_right" },
		{ .name = "sound2", .pattern = "play sound 2", .stop_search_when_match = false, .sound_to_play = "drum_middle" },
		{ .name = "turt", .pattern = "turt", .stop_search_when_match = false, .sound_to_play = "tell2" },
		{ .name = "mezzbreak", .pattern = "has been awakened by", .stop_search_when_match = true, .sound_to_play = "drum_left"}
};

static inline int get_num_triggers()
{
	return (sizeof(triggers) / sizeof(struct trigger));
}


/* sentinel to indicate taking the system default sound attributes */
#define USE_DEFAULT -1000.0
#define USE_DEFAULT_PRIO -1000
struct sound {
	char *name;
	char *file;
	int prio;
	float freq, vol, pan;

};

struct sound sounds[] = {
		{ .name = "drum_left", .file = "../media/drumloop.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = -1.0, .prio = USE_DEFAULT_PRIO },
		{ .name = "drum_right", .file = "../media/drumloop.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = 1.0, .prio = USE_DEFAULT_PRIO },
		{ .name = "drum_middle", .file = "../media/drumloop.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = 0.0, .prio = USE_DEFAULT_PRIO },
		{ .name = "tell2", .file = "../eq_audio/Tell2.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = USE_DEFAULT, .prio = USE_DEFAULT_PRIO },
		{ .name = "alert2", .file = "../eq_audio/Alert2.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = USE_DEFAULT, .prio = USE_DEFAULT_PRIO },
		{ .name = "alert5", .file = "../eq_audio/Alert5.wav", .freq = USE_DEFAULT, .vol = USE_DEFAULT, .pan = USE_DEFAULT, .prio = USE_DEFAULT_PRIO },
};

static inline int get_num_sounds()
{
	return (sizeof(sounds) / sizeof(struct sound));

}
#ifdef __CYGWIN__
#define POS_VAL(pos) pos
#else
#define POS_VAL(pos) pos.__pos
#endif
/* This function works similar to the "tail -f" command */
static void tail_follow(FILE *log_file, off_t *cur_size, char **buffer, size_t *buffer_size)
{
	int ret;
	fpos_t pos;
	struct stat stat_buf;

	fgetpos(log_file, &pos);

	/* the first time through, the next test will be false since cur_size == -1 */
	if (POS_VAL(pos) < *cur_size) {
		getline(buffer, buffer_size, log_file);
		return;
	}
	while (1) {
		/* See if there's any new data since the last time we checked */
		ret = fstat(fileno(log_file), &stat_buf);
		if (ret < 0) {
			fprintf(stderr, "Unable to fstat log file: %s\n",
					strerror(errno));
		}
		if (*cur_size == -1) {
			*cur_size = stat_buf.st_size;
		} else {
			if (stat_buf.st_size < *cur_size) {
				printf("file was truncated!\n");
				exit(1);
			} else {
				if (stat_buf.st_size > *cur_size) {
					getline(buffer, buffer_size, log_file);
					*cur_size = stat_buf.st_size;
					debugmsg("cur_size = %" PRId64 ", pos = %" PRId64 "\n",
							*cur_size,
							POS_VAL(pos));
					return;
				} else {
					/* wait for 10 msec */
					usleep(10 * 1000);
				}
			}
		}
	}
}




/* Before character 27 of every log line is just the time stamp, so skip it */
#define LOG_MSG_START 27


void *logwatcher(void *arg) {
	intptr_t log_file_num = (intptr_t)arg;
	int i, ret;
	size_t buffer_size = 1024;
	off_t cur_size = -1;
	char *buffer;

	buffer = malloc(buffer_size);
	ret = fseek(lfi[log_file_num].file, 0, SEEK_END);
	if (ret < 0) {
		fprintf(stderr, "Unable to seek to end of log file: %s\n",
				strerror(errno));
		exit(1);
	}
	while (1) {
		tail_follow(lfi[log_file_num].file, &cur_size, &buffer, &buffer_size);
		/* chomp off the newline */
		buffer[strlen(buffer) - 1] = '\0';
		debugmsg("got line: %s\n", buffer);
		for (i = 0; i < sizeof(triggers) / sizeof(struct trigger); i++) {
			debugmsg("looking for %s in %s\n", triggers[i].pattern, buffer);
			if (case_insensitive_strstr(&buffer[LOG_MSG_START], triggers[i].pattern)) {
				debugmsg("enqueuing sound %s\n", triggers[i].name);
				enqueue_sound(triggers[i].sound_to_play_id);
				if (triggers[i].stop_search_when_match)
					break;
			}
		}
	}
}


int main(int argc, char *argv[]) {
	FMOD_SYSTEM *system;
	FMOD_SOUND **fmod_sounds;
	FMOD_CHANNEL *channel = 0;
	FMOD_RESULT result;
	unsigned int version;
	int i, j, ret;

	fmod_sounds = malloc(sizeof(FMOD_SOUND *) * get_num_sounds());

	ret = pthread_cond_init(&events.events_available, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events cond object\n");
	}
	ret = pthread_mutex_init(&events.lock, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events lock object\n");
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

	for (i = 0; i < get_num_sounds(); i++) {
		float freq, vol, pan;
		int prio;

		debugmsg("opening sound file %s\n", sounds[i].file);
		result = FMOD_System_CreateSound(system, sounds[i].file,
				FMOD_SOFTWARE, 0, &fmod_sounds[i]);
		ERRCHECK(result);
		result = FMOD_Sound_SetMode(fmod_sounds[i], FMOD_LOOP_OFF);
		ERRCHECK(result);

		result = FMOD_Sound_GetDefaults(fmod_sounds[i], &freq, &vol,
				&pan, &prio);
		ERRCHECK(result);

		/* override defaults as requested */
		if (sounds[i].freq != USE_DEFAULT) {
			debugmsg("setting freq value on %s to %f\n", sounds[i].file, sounds[i].freq);
			freq = sounds[i].freq;
		}
		if (sounds[i].vol != USE_DEFAULT) {
			debugmsg("setting vol value on %s to %f\n", sounds[i].file, sounds[i].vol);
			vol = sounds[i].vol;
		}
		if (sounds[i].pan != USE_DEFAULT) {
			debugmsg("setting pan value on %s to %f\n", sounds[i].file, sounds[i].pan);
			pan = sounds[i].pan;
		}
		if (sounds[i].prio != USE_DEFAULT_PRIO) {
			debugmsg("setting prio value on %s to %d\n", sounds[i].file, sounds[i].prio);
			prio = sounds[i].prio;
		}

		result = FMOD_Sound_SetDefaults(fmod_sounds[i], freq, vol, pan,
						prio);
		ERRCHECK(result);
	}

	lfi = malloc(sizeof(struct log_file_info) * get_num_log_files());
	for (i = 0; i < get_num_log_files(); i++) {
		lfi[i].file = fopen(log_file_names[i], "r");
		if (lfi[i].file == NULL) {
			fprintf(stderr, "fopen return NULL for \"%s\"\n", log_file_names[i]);
		}
		ret = pthread_create(&lfi[i].thread, NULL, logwatcher, (void *)(intptr_t)i);
		if (ret < 0) {
			fprintf(stderr, "Unable to create logwatcher pthread for log file %d\n", i);
			exit(1);
		}
	}

	for (i = 0; i < get_num_triggers(); i++) {
		bool found = false;
		for (j = 0; j < get_num_sounds(); j++) {
			if (strcmp(triggers[i].sound_to_play, sounds[j].name) == 0) {
				found = true;
				triggers[i].sound_to_play_id = j;
			}
		}
		if (!found) {
			fprintf(stderr, "Unable to find sound: %s for trigger: %s\n", triggers[i].sound_to_play, triggers[i].name);
		}
	}

	/*
	 Main loop.
	 */
	pthread_mutex_lock(&events.lock);
	while (1) {
		int sound_id;

		while (events.cur == events.next) {
			debugmsg("event queue is empty\n");
			pthread_cond_wait(&events.events_available, &events.lock);
		}
		events.cur = (events.cur + 1) % NUM_EVENTS;

		sound_id = events.entry[events.cur].sound_id;
		debugmsg("main loop received sound_id %d\n", sound_id);
		if (sound_id < get_num_sounds()) {
			result = FMOD_System_PlaySound(system,
					FMOD_CHANNEL_FREE, fmod_sounds[sound_id], 0, &channel);
			ERRCHECK(result);
		} else {
			fprintf(stderr, "sound_id: %d exceeds the last sound_id: %d\n", sound_id, get_num_sounds() - 1);
			exit(1);
		}
	}

	/*
	 Shut down
	 */
	for (i = 0; i < get_num_sounds(); i++) {
		result = FMOD_Sound_Release(fmod_sounds[i]);
		ERRCHECK(result);
	}
	result = FMOD_System_Close(system);
	ERRCHECK(result);
	result = FMOD_System_Release(system);
	ERRCHECK(result);

	return 0;
}
