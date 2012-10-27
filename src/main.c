/*
 * AudioTriggers
 *
 * Copyright Corey Ashford 2010
 */
#include "../inc/fmod.h"
#include "../inc/fmod_errors.h"
#include <time.h>
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
#include <assert.h>


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlschemas.h>

#define DEBUG 0
#if DEBUG
#define debugmsg(args...) fprintf(stderr, args)
#else
#define debugmsg(args...)
#endif

struct attached_trigger {
	xmlChar *name;
	int trigger_id;
	bool stop_search_on_match;
};

struct logfile {
	xmlChar *file;
	struct attached_trigger *attached_triggers;
	int num_attached_triggers;
};
struct logfile *logfiles;

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

#define NUM_CHANNELS 32
#define NO_SOUND -1

FMOD_CHANNEL *channel[NUM_CHANNELS];

static int num_sounds;
static int num_triggers;
static int num_logfiles;

void _ERRCHECK(FMOD_RESULT result, const char *file, const char *func, int linenum) {
	if (result != FMOD_OK) {
		fprintf(stderr, "FMOD error! (%d) %s at %s:%s:%d\n", result, FMOD_ErrorString(result), file, func, linenum);
		exit(-1);
	}
}

#define ERRCHECK(result) _ERRCHECK(result, __FILE__, __func__, __LINE__)

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

#define NS_IN_SEC 1000000000
static inline struct timespec timespec_diff(const struct timespec * a, const struct timespec *  b)
{
    if(a->tv_nsec < b->tv_nsec)
    {
        struct timespec result = {
            a->tv_sec - b->tv_sec - 1,
            NS_IN_SEC + a->tv_nsec - b->tv_nsec,
        };
        return result;
    } else
    {
        struct timespec result = {
            a->tv_sec - b->tv_sec,
            a->tv_nsec - b->tv_nsec,
        };
        return result;
    }
}

static inline int timespec2ms(const struct timespec * a) {
	return (a->tv_sec * 1000) + (a->tv_nsec / 1000000);
}

/* sentinel to indicate taking the system default sound attributes */
#define USE_DEFAULT -1000.0
#define USE_DEFAULT_PRIO -1000
struct sound {
	xmlChar *name;
	xmlChar *file;
	int prio;
	float vol, pan;
	int min_interval;
	struct timespec timestamp;

};
struct sound *sounds;


static void enqueue_sound(int sound_id)
{
	struct timespec now, elapsed;
	int elapsed_ms;

	if (sound_id == NO_SOUND)
		return;

	clock_gettime(CLOCK_MONOTONIC, &now);
	elapsed = timespec_diff(&now, &(sounds[sound_id].timestamp));
	elapsed_ms = timespec2ms(&elapsed);

	if (elapsed_ms > sounds[sound_id].min_interval) {
		pthread_mutex_lock(&events.lock);
		sounds[sound_id].timestamp = now;
		events.next = (events.next + 1) % NUM_EVENTS;
		debugmsg("cur = %d, next = %d\n", events.cur, events.next);
		if (events.next == events.cur) {
			events.next--;
			fprintf(stderr, "WARNING: event queue overflow! sound dropped\n");
		} else {
			events.entry[events.next].sound_id = sound_id;
		}
		pthread_cond_signal(&events.events_available);
		pthread_mutex_unlock(&events.lock);
	}
}

struct trigger {
	xmlChar *name;
	xmlChar *pattern;
	xmlChar *sound_to_play;
	int sound_to_play_id;

};
struct trigger *triggers;


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
		if (strlen(buffer) > LOG_MSG_START) {
			for (i = 0; i < logfiles[log_file_num].num_attached_triggers; i++) {
				int trigger_id = logfiles[log_file_num].attached_triggers[i].trigger_id;
				debugmsg("looking for %s in %s\n", triggers[trigger_id].pattern, buffer);
				if (case_insensitive_strstr(&buffer[LOG_MSG_START], (char *)triggers[trigger_id].pattern)) {
					debugmsg("enqueuing sound %s\n", triggers[trigger_id].name);
					enqueue_sound(triggers[trigger_id].sound_to_play_id);
					if (logfiles[log_file_num].attached_triggers[i].stop_search_on_match)
						break;
				}
			}
		}
	}
}

int find_free_channel(void)
{
	FMOD_BOOL is_playing;
	FMOD_RESULT result;
	int i;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (channel[i] == NULL) {
			return i;
		};
		result = FMOD_Channel_IsPlaying(channel[i], &is_playing);
		ERRCHECK(result);
		if (!is_playing) {
			return i;
		}
	}
	return -1;
}

#define CONFIG_XML "atconfig.xml"
#define CONFIG_SCHEMA "../XML/AudioTriggers.xsd"

/* the text element is a pseudo element defined by libxml */
#define TEXT_ELT			(xmlChar *)"text"

#define AUDIOTRIGGERS_ELT		(xmlChar *)"audiotriggers"

#define SOUND_ELT 			(xmlChar *)"sound"
#define SOUND_NAME_ATTR 		(xmlChar *)"name"
#define SOUND_FILE_ELT 			(xmlChar *)"file"
#define SOUND_VOL_ELT 			(xmlChar *)"vol"
#define SOUND_PAN_ELT 			(xmlChar *)"pan"
#define SOUND_PRIO_ELT 			(xmlChar *)"priority"
#define SOUND_MIN_INTERVAL_ELT	(xmlChar *)"min_interval"

#define TRIGGER_ELT 			(xmlChar *)"trigger"
#define TRIGGER_NAME_ATTR 		(xmlChar *)"name"
#define TRIGGER_PATTERN_ELT		(xmlChar *)"pattern"
#define TRIGGER_SOUNDTOPLAY_ELT		(xmlChar *)"sound_to_play"
#define TRIGGER_COMMENT_ELT		(xmlChar *)"comment"

#define LOGFILE_ELT			(xmlChar *)"logfile"
#define LOGFILE_FILE_ELT 		(xmlChar *)"file"
#define LOGFILE_ATTACHTRIGGER_ELT	(xmlChar *)"attach_trigger"
#define LOGFILE_ATTACHTRIGGER_STOPSEARCHONMATCH_ELT	(xmlChar *)"stop_search_on_match"
#define LOGFILE_ATTACHTRIGGER_NAME_ATTR	(xmlChar *)"name"

/* This code is from http://wiki.njh.eu/XML-Schema_validation_with_libxml2 */
static int is_valid(const xmlDocPtr doc, const char *schema_filename)
{
	xmlDocPtr schema_doc = xmlReadFile(schema_filename, NULL,
			XML_PARSE_NONET);
	if (schema_doc == NULL) {
		/* the schema cannot be loaded or is not well-formed */
		fprintf(stderr, "Error: Unable to load the schema file \"%s\"\n",
				schema_filename);
		exit(1);
	}
	xmlSchemaParserCtxtPtr parser_ctxt = xmlSchemaNewDocParserCtxt(
			schema_doc);
	if (parser_ctxt == NULL) {
		/* unable to create a parser context for the schema */
		xmlFreeDoc(schema_doc);
		return -2;
	}
	xmlSchemaPtr schema = xmlSchemaParse(parser_ctxt);
	if (schema == NULL) {
		/* the schema itself is not valid */
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(schema_doc);
		return -3;
	}
	xmlSchemaValidCtxtPtr valid_ctxt = xmlSchemaNewValidCtxt(schema);
	if (valid_ctxt == NULL) {
		/* unable to create a validation context for the schema */
		xmlSchemaFree(schema);
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(schema_doc);
		return -4;
	}
	int is_valid = (xmlSchemaValidateDoc(valid_ctxt, doc) == 0);
	xmlSchemaFreeValidCtxt(valid_ctxt);
	xmlSchemaFree(schema);
	xmlSchemaFreeParserCtxt(parser_ctxt);
	xmlFreeDoc(schema_doc);
	/* force the return value to be non-negative on success */
	return is_valid ? 1 : 0;
}

static void open_config_xml(xmlDocPtr *doc)
{
	*doc = xmlParseFile(CONFIG_XML);
	if (*doc == NULL) {
		fprintf(stderr, "Error: unable to parse file \"%s\"\n",
				CONFIG_XML);
		exit(1);
	}
	if (! is_valid(*doc, CONFIG_SCHEMA)) {
		fprintf(stderr, "Error: one or more validation errors in the config file \"%s\"\n",
				CONFIG_XML);
		exit(1);
	}
}

typedef void (*foreach_cb)(xmlNodePtr node);

static void foreach_sibling(xmlNodePtr node, const xmlChar *element, foreach_cb cb)
{
	xmlNodePtr sib;

	for (sib = node; sib; sib = sib->next) {
		debugmsg("node->name: %s\n", sib->name);
		if (xmlStrEqual(sib->name, element))
			cb(sib);
	}
}

static xmlNodePtr get_element(xmlNodePtr node, const xmlChar *element)
{
	xmlNodePtr sib;

	for (sib = node; sib; sib = sib->next) {
		if (xmlStrEqual(sib->name, element))
			return sib;
	}
	return NULL;
}

/* Find an element among siblings, and get the (one) text element within it */
static xmlNodePtr get_element_text(xmlNodePtr node, const xmlChar *element)
{
	xmlNodePtr sib;

	for (sib = node; sib; sib = sib->next) {
		if (xmlStrEqual(sib->name, element)) {
			xmlNodePtr text;
			text = get_element(sib->children, TEXT_ELT);
			if (text == NULL) {
				fprintf(stderr, "unable to find text element for %s\n", (char *)element);
			} else {
				return text;
			}
		}
	}
	return NULL;
}

void process_sound_element(xmlNodePtr node)
{
	xmlNodePtr children = node->children, file, vol, pan, prio, min_interval;
	static int sound_cntr = 0;

	sounds[sound_cntr].name = xmlGetProp(node, SOUND_NAME_ATTR);
	if (sounds[sound_cntr].name == NULL) {
		fprintf(stderr, "Unable to find name attribute on sound element %d\n", sound_cntr + 1);
		exit(1);
	}

	file = get_element_text(children, SOUND_FILE_ELT);
	if (file == NULL) {
		fprintf(stderr, "Unable to find file element in sound element %d\n", sound_cntr + 1);
		exit(1);
	}
	sounds[sound_cntr].file = xmlStrdup(file->content);

	vol = get_element_text(children, SOUND_VOL_ELT);
	if (vol != NULL) {
		debugmsg("vol->content : %s\n", vol->content);
		sscanf((char *)vol->content, "%f", &sounds[sound_cntr].vol);
	} else {
		sounds[sound_cntr].vol = USE_DEFAULT;
	}

	pan = get_element_text(children, SOUND_PAN_ELT);
	if (pan != NULL) {
		sscanf((char *)pan->content, "%f", &sounds[sound_cntr].pan);
	} else {
		sounds[sound_cntr].pan = USE_DEFAULT;
	}

	prio = get_element_text(children, SOUND_PRIO_ELT);
	if (prio != NULL) {
		sscanf((char *)prio->content, "%d", &sounds[sound_cntr].prio);
	} else {
		sounds[sound_cntr].prio = USE_DEFAULT_PRIO;
	}

	min_interval = get_element_text(children, SOUND_MIN_INTERVAL_ELT);
	if (prio != NULL) {
		sscanf((char *)min_interval->content, "%d", &sounds[sound_cntr].min_interval);
	} else {
		sounds[sound_cntr].min_interval = 0;
	}
	sounds[sound_cntr].timestamp.tv_sec = 0;
	sounds[sound_cntr].timestamp.tv_nsec = 0;

	sound_cntr++;
}


static void count_sound_elements(xmlNodePtr node) {
	num_sounds++;
}

static void count_trigger_elements(xmlNodePtr node) {
	num_triggers++;
}

static void count_logfile_elements(xmlNodePtr node) {
	num_logfiles++;
}

static void load_sounds_from_config(xmlNodePtr node)
{
	foreach_sibling(node, SOUND_ELT, count_sound_elements);
	sounds = malloc(sizeof(struct sound) * num_sounds);

	foreach_sibling(node, SOUND_ELT, process_sound_element);
}

void process_trigger_element(xmlNodePtr node)
{
	static int trigger_cntr = 0;
	xmlNodePtr children = node->children, pattern, sound_to_play;

	debugmsg("processing trigger element: %s\n", node->name);

	/* Note that there is a comment element too, but it is ignored by this code */

	triggers[trigger_cntr].name = xmlGetProp(node, TRIGGER_NAME_ATTR);
	if (triggers[trigger_cntr].name == NULL) {
		fprintf(stderr, "Unable to find name attribute on trigger element %d\n", trigger_cntr + 1);
		exit(1);
	}

	pattern = get_element_text(children, TRIGGER_PATTERN_ELT);
	if (pattern == NULL) {
		fprintf(stderr, "Unable to find pattern element in trigger element %d\n", trigger_cntr + 1);
		exit(1);
	}
	triggers[trigger_cntr].pattern = xmlStrdup(pattern->content);

	sound_to_play = get_element_text(children, TRIGGER_SOUNDTOPLAY_ELT);
	if (sound_to_play == NULL) {
		triggers[trigger_cntr].sound_to_play = NULL;
	} else {
		triggers[trigger_cntr].sound_to_play = xmlStrdup(sound_to_play->content);
	}


	trigger_cntr++;
}

static void load_triggers_from_config(xmlNodePtr node)
{
	foreach_sibling(node, TRIGGER_ELT, count_trigger_elements);
	triggers = malloc(sizeof(struct trigger) * num_triggers);

	foreach_sibling(node, TRIGGER_ELT, process_trigger_element);
}

static int attach_trigger_cntr;
static int logfile_cntr;

void count_attach_trigger_elements(xmlNodePtr node)
{
	attach_trigger_cntr++;
}

void process_attach_trigger_element(xmlNodePtr node)
{
	xmlChar *name = xmlGetProp(node, LOGFILE_ATTACHTRIGGER_NAME_ATTR);
	xmlNodePtr children = node->children, stop_search_on_match;

	if (name == NULL) {
		fprintf(stderr, "Unable to find name attribute on attach_trigger element %d\n", logfile_cntr + 1);
		exit(1);
	}

	logfiles[logfile_cntr].attached_triggers[attach_trigger_cntr].name = name;

	stop_search_on_match = get_element(children, LOGFILE_ATTACHTRIGGER_STOPSEARCHONMATCH_ELT);
	if (stop_search_on_match == NULL) {
		logfiles[logfile_cntr].attached_triggers[attach_trigger_cntr].stop_search_on_match = false;
		debugmsg("setting stop search on match to false\n");
	} else {
		logfiles[logfile_cntr].attached_triggers[attach_trigger_cntr].stop_search_on_match = true;
		debugmsg("setting stop search on match to true\n");
	}
	attach_trigger_cntr++;
}

void process_logfile_element(xmlNodePtr node)
{
	xmlNodePtr children = node->children, file;

	debugmsg("processing logfile element: %s\n", node->name);
	file = get_element_text(children, LOGFILE_FILE_ELT);
	if (file == NULL) {
		fprintf(stderr, "Unable to find file element in logfile element %d\n", logfile_cntr + 1);
		exit(1);
	}
	logfiles[logfile_cntr].file = xmlStrdup(file->content);
	debugmsg("logfile found: %s\n", file->content);

	/* malloc space for the attached trigger pointers */
	attach_trigger_cntr = 0;
	foreach_sibling(children, LOGFILE_ATTACHTRIGGER_ELT, count_attach_trigger_elements);
	logfiles[logfile_cntr].attached_triggers = malloc(sizeof(struct attached_trigger) * attach_trigger_cntr);
	logfiles[logfile_cntr].num_attached_triggers = attach_trigger_cntr;

	attach_trigger_cntr = 0;
	foreach_sibling(children, LOGFILE_ATTACHTRIGGER_ELT, process_attach_trigger_element);

	logfile_cntr++;
}

static void load_logfiles_from_config(xmlNodePtr node)
{
	foreach_sibling(node, LOGFILE_ELT, count_logfile_elements);
	logfiles = malloc(sizeof(struct logfile) * num_logfiles);

	foreach_sibling(node, LOGFILE_ELT, process_logfile_element);
}

static void close_config_xml(xmlDocPtr doc)
{
	xmlFreeDoc(doc);
}

static void init_xml_lib(void)
{
	/* Init libxml */
	xmlInitParser();
	LIBXML_TEST_VERSION
}

static void init_sound_system(FMOD_SYSTEM **system)
{
	FMOD_RESULT result;
	unsigned int version;

	result = FMOD_System_Create(system);
	ERRCHECK(result);

	result = FMOD_System_GetVersion(*system, &version);
	ERRCHECK(result);

	if (version < FMOD_VERSION) {
		printf("Error!  You are using an old version of FMOD %08x.  This program requires %08x\n",
			version, FMOD_VERSION);
		exit(1);
	}

	result = FMOD_System_Init(*system, 32, FMOD_INIT_NORMAL, NULL);
	ERRCHECK(result);
}

static void open_all_sounds(FMOD_SYSTEM *system, FMOD_SOUND **fmod_sounds) {
	FMOD_RESULT result;
	int i;

	for (i = 0; i < num_sounds; i++) {
		float freq, vol, pan;
		int prio;

		debugmsg("opening sound file %s\n", sounds[i].file);
		result = FMOD_System_CreateSound(system, (char *)sounds[i].file,
				FMOD_SOFTWARE, 0, &fmod_sounds[i]);
		if (result != FMOD_OK) {
			fprintf(stderr, "Unable to open sound file \"%s\"\n", sounds[i].file);
			exit(1);
		}
		result = FMOD_Sound_SetMode(fmod_sounds[i], FMOD_LOOP_OFF);
		ERRCHECK(result);

		result = FMOD_Sound_GetDefaults(fmod_sounds[i], &freq, &vol,
				&pan, &prio);
		ERRCHECK(result);

		/* override defaults (except freq) as requested */
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
}

static void open_all_logfiles(void)
{
	int i, ret;

	for (i = 0; i < num_logfiles; i++) {
		lfi[i].file = fopen((char *)logfiles[i].file, "r");
		if (lfi[i].file == NULL) {
			fprintf(stderr, "Unable to open logfile \"%s\": %s\n", logfiles[i].file, strerror(errno));
			exit(1);
		}
		ret = pthread_create(&lfi[i].thread, NULL, logwatcher, (void *)(intptr_t)i);
		if (ret < 0) {
			fprintf(stderr, "Unable to create logwatcher pthread for log file %d\n", i);
			exit(1);
		}
	}
}

static void match_triggers_with_sounds(void)
{
	int i, j;

	for (i = 0; i < num_triggers; i++) {
		bool found = false;
		if (triggers[i].sound_to_play == NULL) {
			triggers[i].sound_to_play_id = NO_SOUND;
			continue;
		}
		for (j = 0; j < num_sounds; j++) {
			if (xmlStrEqual(triggers[i].sound_to_play, sounds[j].name)) {
				found = true;
				triggers[i].sound_to_play_id = j;
			}
		}
		if (!found) {
			fprintf(stderr, "Unable to find sound: %s for trigger: %s\n", triggers[i].sound_to_play, triggers[i].name);
			exit(1);
		}
	}
}

static void match_logfiles_with_triggers(void)
{
	int i, j, k;

	for (i = 0; i < num_logfiles; i++) {
		for (j = 0; j < logfiles[i].num_attached_triggers; j++) {
			bool found = false;
			for (k = 0; k < num_triggers; k++) {
				if (xmlStrEqual(logfiles[i].attached_triggers[j].name, triggers[k].name)) {
					found = true;
					logfiles[i].attached_triggers[j].trigger_id = k;
				}
			}
			if (!found) {
				fprintf(stderr, "Unable to find trigger: %s for logfile: %s\n", logfiles[i].attached_triggers[j].name, logfiles[i].file);
				exit(1);
			}
		}
	}
}


static void release_all_sounds(FMOD_SOUND **fmod_sounds)
{
	int i;
	FMOD_RESULT result;

	for (i = 0; i < num_sounds; i++) {
		result = FMOD_Sound_Release(fmod_sounds[i]);
		ERRCHECK(result);
	}
}

static void close_sound_system(FMOD_SYSTEM *system)
{
	FMOD_RESULT result;

	result = FMOD_System_Close(system);
	ERRCHECK(result);
	result = FMOD_System_Release(system);
	ERRCHECK(result);
}

static void print_thankyou(void) {
	printf("---------------------------------------------------\n");
	printf("--              AudioTriggers+ 0.1               --\n");
	printf("--                                               --\n");
	printf("--  Implementation uses the FMODex Sound System  --\n");
	printf("--    from Firelight Technologies @ FMOD.org     --\n");
	printf("---------------------------------------------------\n");
}

int main(int argc, char *argv[]) {
	FMOD_SYSTEM *system;
	FMOD_SOUND **fmod_sounds;
	FMOD_RESULT result;
	xmlDocPtr doc;
	xmlNodePtr node;
	int ret;

	ret = pthread_cond_init(&events.events_available, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events cond object\n");
	}
	ret = pthread_mutex_init(&events.lock, NULL);
	if (ret < 0) {
		fprintf(stderr, "Unable to initialize the events lock object\n");
	}

	init_sound_system(&system);
	init_xml_lib();

	open_config_xml(&doc);
	node = doc->children;
	if (!xmlStrEqual(node->name, AUDIOTRIGGERS_ELT)) {
		fprintf(stderr, "root element of %s is not audiotriggers\n", CONFIG_XML);
		exit(1);
	}
	node = node->children;
	load_sounds_from_config(node);
	load_triggers_from_config(node);
	load_logfiles_from_config(node);
	close_config_xml(doc);

	fmod_sounds = malloc(sizeof(FMOD_SOUND *) * num_sounds);
	open_all_sounds(system, fmod_sounds);

	lfi = malloc(sizeof(struct log_file_info) * num_logfiles);
	open_all_logfiles();

	match_triggers_with_sounds();

	match_logfiles_with_triggers();

	print_thankyou();
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
		if (sound_id < num_sounds) {
			int free_channel;

			free_channel = find_free_channel();
			if (free_channel == -1) {
				fprintf(stderr, "WARNING: No free channels! Dropping sound\n");
			} else {
				result = FMOD_System_PlaySound(system,
						FMOD_CHANNEL_FREE, fmod_sounds[sound_id], 0, &channel[free_channel]);
				ERRCHECK(result);
			}
		} else {
			fprintf(stderr, "sound_id: %d exceeds the last sound_id: %d\n", sound_id, num_sounds - 1);
			exit(1);
		}
	}

	release_all_sounds(fmod_sounds);
	close_sound_system(system);

	return 0;
}
