#if !defined(__APPLE__) || !defined(__MACH__)
#error Only Mac OSX is currently supported.
#endif

#define _DARWIN_FEATURE_64_BIT_INODE

#import <assert.h>
#import <strings.h>
#import <stdio.h>
#import <stdlib.h>
#import <stdbool.h>
#import <inttypes.h>
#import <string.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/mount.h>
#import <getopt.h>
#import <sysexits.h>
#import <sys/types.h>
#import <pwd.h>

// TODO: Check for errors.

static const int EX_NOTFOUND = 1;
static const char *progname;

static struct option longopts[] = {
	{
		.name = "quiet",
		.has_arg = no_argument,
		.flag = NULL,
		.val = 'q'
	},
	{ "type", required_argument, NULL, 't' },
	{ "help", no_argument, NULL, 'h' },
	{ "from", required_argument, NULL, 'f' },
	{ "on", required_argument, NULL, 'o' },
};

static int usage( FILE *stream ) {
	return fprintf(stream, "Usage: %s [-hq] [-t fstype] [-f mntfrom] [-o mnton] [--help] [--quiet] [--type=fstype] [--from=mntfrom] [--on=mnton] [format]\n", progname);
}

typedef struct {
	char *str;
	bool heap;
} component_t;

static char *get_formatted_string( const char *format, struct statfs *stat ) {
	assert(format != NULL);
	assert(stat != NULL);
	size_t len = strlen(format) + 1;
	char fmt[len];
	strcpy(fmt, format);
	size_t ncomponents = 1;
	for( char *cptr = fmt; *cptr != '\0'; cptr++ )
		if( *cptr == '%' ) ncomponents += 2;

	component_t components[ncomponents];
	bzero(components, sizeof(component_t) * ncomponents);
	component_t *next_component = components;
	(next_component++)->str = fmt;
	
	for( size_t i = 0; i < len; i++ ) {
		if( fmt[i] == '%' ) {
			fmt[i++] = '\0';
			switch( fmt[i] ) {
				case 't':
					(next_component++)->str = stat->f_fstypename;
					break;
				case 'f':
					(next_component++)->str = stat->f_mntfromname;
					break;
				case 'o':
					(next_component++)->str = stat->f_mntonname;
					break;
				case 'B':
					asprintf(&next_component->str, "%" PRIu32, stat->f_bsize);
					(next_component++)->heap = true;
					break;
				case 'I':
					asprintf(&next_component->str, "%" PRId32, stat->f_iosize);
					(next_component++)->heap = true;
					break;
				case 'b':
					asprintf(&next_component->str, "%" PRIu64, stat->f_blocks);
					(next_component++)->heap = true;
					break;
				case 'F':
					asprintf(&next_component->str, "%" PRIu64, stat->f_bfree);
					(next_component++)->heap = true;
					break;
				case 'a':
					asprintf(&next_component->str, "%" PRIu64, stat->f_bavail);
					(next_component++)->heap = true;
					break;
				case 'n':
					asprintf(&next_component->str, "%" PRIu64, stat->f_files);
					(next_component++)->heap = true;
					break;
				case 'e':
					asprintf(&next_component->str, "%" PRIu64, stat->f_ffree);
					(next_component++)->heap = true;
					break;
				case 'S':
					asprintf(&next_component->str, "%" PRId32, stat->f_fsid.val[0]);
					(next_component++)->heap = true;
					break;
				case 'T':
					asprintf(&next_component->str, "%" PRId32, stat->f_fsid.val[1]);
					(next_component++)->heap = true;
					break;
				case 'U':
					asprintf(&next_component->str, "%" PRId64, *((int64_t *) &stat->f_fsid));
					(next_component++)->heap = true;
					break;
				case 'O':
					asprintf(&next_component->str, "%" PRIdMAX, (intmax_t) stat->f_owner);
					(next_component++)->heap = true;
					break;
				case 'P':
					(next_component++)->str = getpwuid(stat->f_owner)->pw_name;
					break;
				case 'p':
					asprintf(&next_component->str, "%" PRIu32, stat->f_type);
					(next_component++)->heap = true;
					break;
				case 'g':
					asprintf(&next_component->str, "%" PRIu32, stat->f_flags);
					(next_component++)->heap = true;
					break;
				case 's':
					asprintf(&next_component->str, "%" PRIu32, stat->f_fssubtype);
					(next_component++)->heap = true;
					break;
				case '%':
					(next_component++)->str = "%";
					break;
				case '\0':
					fprintf(stderr, "Missing format specifier at end of format string\n");
					exit(EX_DATAERR);
				default:
					fprintf(stderr, "Unknown format specifier '%c'\n", fmt[i]);
					exit(EX_DATAERR);
			}
			fmt[i] = '\0';
			(next_component++)->str = &fmt[i + 1];
		}
	}
	
	size_t size = 1;
	for( size_t i = 0; i < ncomponents; i++ ) size += strlen(components[i].str);
	char *ret = calloc(size, sizeof(char));
	ret[0] = '\0';
	char *ret_next = ret;
	for( size_t i = 0; i < ncomponents; i++ ) {
		ret_next = strcat(ret_next, components[i].str);
		if( components[i].heap ) free(components[i].str);
	}
	return ret;
}

int main( int argc, char *argv[] ) {
	progname = argv[0];
	char *type = NULL, *from = NULL, *on = NULL;

	int ch;
	while( (ch = getopt_long(argc, argv, "qht:f:o:", longopts, NULL)) != -1 ) {
		switch( ch ) {
			case 't':
				type = optarg;
				break;
			case 'f':
				from = optarg;
				break;
			case 'o':
				on = optarg;
				break;
			case 'q':
				break;
			case 'h':
				usage(stdout);
				exit(EX_OK);
			case '?':
			case ':':
			default:
				usage(stderr);
				exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if( argc > 1 ) {
		usage(stderr);
		exit(EX_USAGE);
	}

	struct statfs *mntbuf;
	int mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	int retval = EX_NOTFOUND;
	for( int i = 0; i < mntsize; i++ ) {
		if( type != NULL && strncmp(type, mntbuf[i].f_fstypename, MFSTYPENAMELEN) != 0 ) continue;
		if( from != NULL && strncmp(from, mntbuf[i].f_mntfromname, MAXPATHLEN) != 0 ) continue;
		if( on != NULL && strncmp(on, mntbuf[i].f_mntonname, MAXPATHLEN) != 0 ) continue;
		if( argc == 1 ) {
			char *str = get_formatted_string(argv[0], &mntbuf[i]);
			printf("%s\n", str);
			free(str);
		}
		retval = EX_OK;
	}
	return retval;
}
