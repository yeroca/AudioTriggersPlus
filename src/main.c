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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

char *log_file_names[] = {
		"/home/corey/log1",
		"/home/corey/log2"
};

struct log_file_info {
	int fd;
	FILE *file;
};

struct log_file_info *lfi;

static int num_log_files(void)
{
	return sizeof(log_file_names) / sizeof(char *);
}

void ERRCHECK(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}
}

int get_readable_fd(fd_set *fds, int max_fd) {
	int i;

	for (i = 0; i <= max_fd; i++) {
		if (FD_ISSET(i, fds)) {
			return i;
		}
	}
	return -1;
}

static size_t buf_size = 1024;
static char* buffer = NULL;

int get_log_line(fd_set *fds, int max_fd, char **line, int *log_file_num) {

	int ret, readable_fd, i;
	FILE *readable_file;
	fd_set read_fds = *fds;
	FILE *file;
	bool found = false;

	ret = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, NULL);
	fprintf(stderr, "pselect returned %d\n", ret);

	if (ret < 0) {
		printf("pselect returned error: %d\n", ret);
		exit(1);
	}
	readable_fd = get_readable_fd(&read_fds, max_fd);
	if (readable_fd < 0) {
		fprintf(stderr, "get_readable_fds returned %d\n", readable_fd);
		exit(1);
	}
	for (i = 0; i < num_log_files(); i++) {
		if (lfi[i].fd == readable_fd) {
			found = true;
			break;
		}
	}
	if (!found) {
		fprintf(stderr, "unable to find fd %d in lfi array\n", readable_fd);
		exit(1);
	}

	if (buffer == NULL)
		buffer = malloc(buf_size);

	ret = getline(&buffer, &buf_size, lfi[i].file);

	/* trim off new line */
	fprintf(stderr, "buf_size = %d, ret = %d, strlen(buffer) = %d, line read: %s\n",
			buf_size, ret, strlen(buffer), buffer);

	*log_file_num = i;
	*line = buffer;
}

struct trigger {
	char *pattern;
	int sound_to_play;
};

struct trigger triggers[] = { { "play sound 1", 1 }, { "play sound 2", 2 }, { "play sound 3", 3 } };

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
	char tail_cmd[1024];

	FD_ZERO(&log_fd_set);

	lfi = malloc(sizeof(struct log_file_info) * num_log_files());

	for (i = 0; i < num_log_files(); i++) {
		sprintf(tail_cmd, "/usr/bin/tail -f %s", log_file_names[i]);
fprintf(stderr, "attempting to open pipe for command: %s\n", tail_cmd);
		lfi[i].file = popen(tail_cmd, "r");
		if (lfi[i].file == NULL) {
			fprintf(stderr, "popen return NULL for \"%s\"\n", tail_cmd);
		}
		lfi[i].fd = fileno(lfi[i].file);
		if (lfi[i].fd < 0) {
			fprintf(stderr, "Unable to convert FILE pointer to fd\n");
			exit(1);
		}
		if (lfi[i].fd > max_fd)
			max_fd = lfi[i].fd;
fprintf(stderr, "setting fd %d\n", lfi[i].fd);
		FD_SET(lfi[i].fd, &log_fd_set);
	}

	/*
	 Create a System object and initialize.
	 */
	result = FMOD_System_Create(&system);
	ERRCHECK(result);

	result = FMOD_System_GetVersion(system, &version);
	ERRCHECK(result);

	if (version < FMOD_VERSION) {
		printf(
				"Error!  You are using an old version of FMOD %08x.  This program requires %08x\n",
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
	while (1) {
		get_log_line(&log_fd_set, max_fd, &line, &log_num);
fprintf(stderr, "log line came from file %d\n", log_num);
		for (i = 0; i < sizeof(triggers) / sizeof(struct trigger); i++) {
			if (strstr(line, triggers[i].pattern)) {
				switch (triggers[i].sound_to_play) {
				case 1:
					result = FMOD_System_PlaySound(system, FMOD_CHANNEL_FREE,
							sound1, 0, &channel);
					ERRCHECK(result);
					break;
				case 2:
					result = FMOD_System_PlaySound(system, FMOD_CHANNEL_FREE,
							sound2, 0, &channel);
					ERRCHECK(result);
					break;
				case 3:
					result = FMOD_System_PlaySound(system, FMOD_CHANNEL_FREE,
							sound3, 0, &channel);
					ERRCHECK(result);
					break;
				}
			}
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

