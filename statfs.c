#define _DARWIN_FEATURE_64_BIT_INODE

#import <stdio.h>
#import <stdlib.h>
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

int usage( char *progname, FILE *stream ) {
	return fprintf(stderr, "Usage: %s [-hq] [-t fstype] [-f mntfrom] [-o mnton] [--help] [--quiet] [--type=fstype] [--from=mntfrom] [--on=mnton] [format]\n", progname);
}

int main( int argc, char *argv[] ) {
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
				printf("-q\n");
				break;
			case 'h':
				printf("-h\n");
				usage(argv[0], stdout);
				exit(EX_OK);
			case '?':
			case ':':
			default:
				usage(argv[0], stderr);
				exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if( argc > 1 ) {
		usage(argv[0], stderr);
		exit(EX_USAGE);
	}

	struct statfs *mntbuf;
	int mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for( int i = 0; i < mntsize; i++ ) {
		if( type != NULL && strncmp(type, mntbuf[i].f_fstypename, MFSTYPENAMELEN) != 0 ) continue;
		if( from != NULL && strncmp(from, mntbuf[i].f_mntfromname, MAXPATHLEN) != 0 ) continue;
		if( on != NULL && strncmp(on, mntbuf[i].f_mntonname, MAXPATHLEN) != 0 ) continue;
		return EX_OK;
	}
	return EX_NOTFOUND;
}
