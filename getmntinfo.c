#if !defined(__APPLE__) || !defined(__MACH__)
#error Only Mac OSX is currently supported.
#endif

#define _DARWIN_FEATURE_64_BIT_INODE

#import <errno.h>
#import <stdarg.h>
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

// Fatal wrappers around libc functions.
static int evfprintf( FILE *stream, const char *fmt, va_list ap ) {
	int ret = vfprintf(stream, fmt, ap);
	if( ret < 0 ) {
		perror("vfprintf");
		exit(EX_OSERR);
	}
	return ret;
}

static int efprintf( FILE *stream, const char *fmt, ... ) {
	va_list ap;
	va_start(ap, fmt);
	int ret = evfprintf(stream, fmt, ap);
	va_end(ap);
	return ret;
}

static int eprintf( const char *fmt, ... ) {
	va_list ap;
	va_start(ap, fmt);
	int ret = evfprintf(stdout, fmt, ap);
	va_end(ap);
	return ret;
}

static int evasprintf( char **ret, const char *fmt, va_list ap ) {
	int retval = vasprintf(ret, fmt, ap);
	if( retval < 0 ) {
		perror("vasprintf");
		exit(EX_OSERR);
	}
	return retval;
}

static struct passwd *egetpwuid( uid_t uid ) {
	errno = 0;
	struct passwd *ret = getpwuid(uid);
	if( ret == NULL && errno != 0 ) {
		perror("getpwuid");
		exit(EX_OSERR);
	}
	return ret;
}

static void *ecalloc( size_t count, size_t size ) {
	void *ret = calloc(count, size);
	if( ret == NULL ) {
		perror("calloc");
		exit(EX_OSERR);
	}
	return ret;
}

static int egetmntinfo( struct statfs **mntbufp, int flags ) {
	int ret = getmntinfo(mntbufp, flags);
	if( ret == 0 ) {
		perror("getmntinfo");
		exit(EX_OSERR);
	}
	return ret;
}
// End libc function wrappers.

enum {
	EX_NOTFOUND = 1
};
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

static void usage( FILE *stream ) {
	efprintf(stream, "Usage: %s [-hq] [-t fstype] [-f mntfrom] [-o mnton] [--help] [--quiet] [--type=fstype] [--from=mntfrom] [--on=mnton] [format]\n", progname);
}

typedef struct {
	char *str;
	size_t len;
	bool heap : 1;
	bool size_known : 1;
} component_t;

static size_t component_printf( component_t *component, const char *fmt, ... ) {
	va_list ap;
	va_start(ap, fmt);
	component->len = evasprintf(&component->str, fmt, ap);
	va_end(ap);
	component->size_known = true;
	component->heap = true;
	return component->len;
}

static char *get_formatted_string( const char *format, const struct statfs *stat ) {
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

	size_t size = 1;
	for( size_t i = 0; i < len; i++ ) {
		if( fmt[i] == '%' ) {
			fmt[i++] = '\0';
			switch( fmt[i] ) {
				case 't':
					(next_component++)->str = (char *) stat->f_fstypename;
					break;
				case 'f':
					(next_component++)->str = (char *) stat->f_mntfromname;
					break;
				case 'o':
					(next_component++)->str = (char *) stat->f_mntonname;
					break;
				case 'B':
					size += component_printf(next_component++, "%" PRIu32, stat->f_bsize);
					break;
				case 'I':
					size += component_printf(next_component++, "%" PRId32, stat->f_iosize);
					break;
				case 'b':
					size += component_printf(next_component++, "%" PRIu64, stat->f_blocks);
					break;
				case 'F':
					size += component_printf(next_component++, "%" PRIu64, stat->f_bfree);
					break;
				case 'a':
					size += component_printf(next_component++, "%" PRIu64, stat->f_bavail);
					break;
				case 'n':
					size += component_printf(next_component++, "%" PRIu64, stat->f_files);
					break;
				case 'e':
					size += component_printf(next_component++, "%" PRIu64, stat->f_ffree);
					break;
				case 'S':
					size += component_printf(next_component++, "%" PRId32, stat->f_fsid.val[0]);
					break;
				case 'T':
					size += component_printf(next_component++, "%" PRId32, stat->f_fsid.val[1]);
					break;
				case 'U':
					size += component_printf(next_component++, "%" PRId64, *((int64_t *) &stat->f_fsid));
					break;
				case 'O':
					size += component_printf(next_component++, "%" PRIdMAX, (intmax_t) stat->f_owner);
					break;
				case 'P':
					(next_component++)->str = egetpwuid(stat->f_owner)->pw_name;
					break;
				case 'p':
					size += component_printf(next_component++, "%" PRIu32, stat->f_type);
					break;
				case 'g':
					size += component_printf(next_component++, "%" PRIu32, stat->f_flags);
					break;
				case 's':
					size += component_printf(next_component++, "%" PRIu32, stat->f_fssubtype);
					break;
				case '%':
					(next_component++)->str = (char *) "%";
					break;
				case '\0':
					efprintf(stderr, "Missing format specifier at end of format string\n");
					exit(EX_DATAERR);
				default:
					efprintf(stderr, "Unknown format specifier '%c'\n", fmt[i]);
					exit(EX_DATAERR);
			}
			fmt[i] = '\0';
			(next_component++)->str = &fmt[i + 1];
		}
	}

	for( size_t i = 0; i < ncomponents; i++ ) {
		if( !components[i].size_known ) {
			components[i].len = strlen(components[i].str);
			size += components[i].len;
		}
	}

	char *ret = ecalloc(size, sizeof(char));
	char *ret_next = ret;
	for( size_t i = 0; i < ncomponents; i++ ) {
		size_t count = components[i].len * sizeof(char);
		memcpy(ret_next, components[i].str, count);
		ret_next += count;
		if( components[i].heap ) free(components[i].str);
	}
	*ret_next = '\0';
	return ret;
}

int main( int argc, char *argv[] ) {
	progname = argv[0];
	char *type = NULL, *from = NULL, *on = NULL;
	bool quiet = false;

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
				quiet = true;
				break;
			case 'h':
				usage(stdout);
				exit(EX_OK);
			case '?':
			case ':':
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
	char *format = argc == 1 ? argv[0] : "%f on %o (%t)";

	struct statfs *mntbuf;
	int mntsize = egetmntinfo(&mntbuf, MNT_NOWAIT);
	int retval = EX_NOTFOUND;
	for( int i = 0; i < mntsize; i++ ) {
		if( type != NULL && strncmp(type, mntbuf[i].f_fstypename, MFSTYPENAMELEN) != 0 ) continue;
		if( from != NULL && strncmp(from, mntbuf[i].f_mntfromname, MAXPATHLEN) != 0 ) continue;
		if( on != NULL && strncmp(on, mntbuf[i].f_mntonname, MAXPATHLEN) != 0 ) continue;
		if( !quiet ) {
			char *str = get_formatted_string(format, &mntbuf[i]);
			eprintf("%s\n", str);
			free(str);
		}
		retval = EX_OK;
	}
	return retval;
}
