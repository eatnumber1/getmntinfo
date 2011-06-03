#if !defined(__APPLE__) || !defined(__MACH__)
#error Only Mac OSX is currently supported.
#endif

#define _DARWIN_FEATURE_64_BIT_INODE

#import <assert.h>
#import <stdio.h>
#import <stdlib.h>
#import <stdbool.h>
#import <string.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/mount.h>
#import <getopt.h>
#import <sysexits.h>

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

static void print_formatted_str( const char *format, struct statfs *stat ) {
	assert(format != NULL);
	assert(stat != NULL);
	size_t len = strlen(format) + 1;
	char fmt[len];
	strcpy(fmt, format);
	size_t ncomponents = 2;
	for( char *cptr = fmt; *cptr != '\0'; cptr++ )
		if( *cptr == '%' ) ncomponents += 2;

	char *components[ncomponents];
	char **next_component = components;
	*next_component++ = fmt;
	for( size_t i = 0; i < len; i++ ) {
		if( fmt[i] == '%' ) {
			fmt[i++] = '\0';
			switch( fmt[i] ) {
				case 't':
					*next_component++ = stat->f_fstypename;
					break;
				case 'f':
					*next_component++ = stat->f_mntfromname;
					break;
				case 'o':
					*next_component++ = stat->f_mntonname;
					break;
				case '%':
					*next_component++ = "%";
					break;
				case '\0':
					fprintf(stderr, "Missing format specifier at end of format string\n");
					exit(EX_DATAERR);
				default:
					fprintf(stderr, "Unknown format specifier '%c'\n", fmt[i]);
					exit(EX_DATAERR);
			}
			fmt[i] = '\0';
			*next_component++ = &fmt[i + 1];
		}
	}
	*next_component++ = "\n";
	
	for( size_t i = 0; i < ncomponents; i++ ) printf("%s", components[i]);
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
		if( argc == 1 ) print_formatted_str(argv[0], &mntbuf[i]);
		retval = EX_OK;
	}
	return retval;
}
