#define _DARWIN_FEATURE_64_BIT_INODE

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
static size_t longopts_len = sizeof(longopts) / sizeof(struct option);

static const char *progname;

static int usage( FILE *stream ) {
	return fprintf(stream, "Usage: %s [-hq] [-t fstype] [-f mntfrom] [-o mnton] [--help] [--quiet] [--type=fstype] [--from=mntfrom] [--on=mnton] [format]\n", progname);
}

static void print_format( const char *fmt, struct statfs *stat ) {
	// TODO: Don't print incomplete buffers in case of an error.
	bool formatting = false;
	size_t len = strlen(fmt);
	for( size_t i = 0; i < len; i++ ) {
		if( formatting ) {
			switch( fmt[i] ) {
				case 't':
					printf("%s", stat->f_fstypename);
					break;
				case 'f':
					printf("%s", stat->f_mntfromname);
					break;
				case 'o':
					printf("%s", stat->f_mntonname);
					break;
				case '%':
					printf("%c", '%');
					break;
				default:
					fprintf(stderr, "Unknown format specifier '%c'\n", fmt[i]);
					exit(EX_DATAERR);
			}
			formatting = false;
		} else if( fmt[i] == '%' ) {
			if( i == len - 1 ) {
				fprintf(stderr, "Missing format specifier\n");
				exit(EX_DATAERR);
			}
			formatting = true;
		} else {
			printf("%c", fmt[i]);
		}
	}
	printf("\n");
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
		if( argc == 1 ) print_format(argv[0], &mntbuf[i]);
		retval = EX_OK;
	}
	return retval;
}
