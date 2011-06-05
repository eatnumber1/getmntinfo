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

#if 0
static const struct mount_option_t {
	const intmax_t option;
	const char * const name;
} mount_options[] = {
	{
		.option = MNT_RDONLY,
		.name = "readonly"
	},
	{ MNT_SYNCHRONOUS, "synchronous" },
	{ MNT_NOEXEC, "noexec" },
	{ MNT_NOSUID, "nosuid" },
	{ MNT_NODEV, "nodev" },
	{ MNT_UNION, "union" },
	{ MNT_ASYNC, "async" },
	{ MNT_EXPORTED, "exported" },
	{ MNT_LOCAL, "local" },
	{ MNT_QUOTA, "quota" },
	{ MNT_ROOTFS, "rootfs" },
	{ MNT_DOVOLFS, "volfs" },
	{ MNT_DONTBROWSE, "dontbrowse" },
	{ MNT_UNKNOWNPERMISSIONS, "unknownpermissions" },
	{ MNT_AUTOMOUNTED, "automounted" },
	{ MNT_JOURNALED, "journaled" },
	{ MNT_DEFWRITE, "deferwrite" },
	{ MNT_MULTILABEL, "multilabel" },
};
static const size_t mout_options_size = sizeof(mount_options) / sizeof(struct mount_option_t);
#endif

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
	if( ret == NULL ) {
		if( errno != 0 ) {
			perror("getpwuid");
			exit(EX_OSERR);
		} else {
			// OS X is non-compliant with POSIX.1-2008, so this code will never run.
			efprintf(stderr, "No passwd entry found for uid %jd\n", uid);
			exit(EX_DATAERR);
		}
	}
	return ret;
}

static struct passwd *egetpwnam( const char *login ) {
	errno = 0;
	struct passwd *ret = getpwnam(login);
	if( ret == NULL ) {
		if( errno != 0 ) {
			perror("getpwnam");
			exit(EX_OSERR);
		} else {
			// OS X is non-compliant with POSIX.1-2008, so this code will never run.
			efprintf(stderr, "No passwd entry found for user \"%s\"\n", login);
			exit(EX_DATAERR);
		}
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

typedef struct {
	struct option longopt;
	const char *helpstr;
	const char *argname;
} help_option_t;

static help_option_t help_options[] = {
	{
		.longopt = {
			.name = "quiet",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'q'
		},
		.helpstr = "Produce no output",
	},
	{ { "help", no_argument, NULL, 'h' }, "This help message", NULL },
	{ { "bsize", required_argument, NULL, 'B' }, "Fundamental filesystem block size", "SIZE" },
	{ { "iosize", required_argument, NULL, 'I' }, "Optimal transfer block size", "SIZE" },
	{ { "blocks", required_argument, NULL, 'b' }, "Total data blocks in filesystem", "COUNT" },
	{ { "bfree", required_argument, NULL, 'F' }, "Free blocks in filesystem", "COUNT" },
	{ { "bavail", required_argument, NULL, 'a' }, "Free blocks avail to non-superuser", "COUNT" },
	{ { "files", required_argument, NULL, 'n' }, "Total file nodes in filesystem", "COUNT" },
	{ { "ffree", required_argument, NULL, 'e' }, "Free file nodes in filesystem", "COUNT" },
	{ { "fsid", required_argument, NULL, 'U' }, "Filesystem identifier", "ID" },
	{ { "fsid0", required_argument, NULL, 'S' }, "Top four bytes of filesystem identifier", "ID" },
	{ { "fsid1", required_argument, NULL, 'T' }, "Bottom four bytes of filesystem identifier", "ID" },
	{ { "owner", required_argument, NULL, 'O' }, "User that mounted the filesystem", "USER" },
	{ { "flags", required_argument, NULL, 'g' }, "Copy of mount exported flags", "FLAGS" },
	{ { "fssubtype", required_argument, NULL, 's' }, "Filesystem sub-type (flavor)", "TYPE" },
	{ { "type", required_argument, NULL, 't' }, "Type of filesystem", "TYPE" },
	{ { "mntonname", required_argument, NULL, 'o' }, "Directory on which mounted", "DIR" },
	{ { "mntfromname", required_argument, NULL, 'f' }, "Mounted filesystem", "NAME" },
};
static size_t help_options_len = sizeof(help_options) / sizeof(help_option_t);

typedef union {
	intmax_t intmax;
	uintmax_t uintmax;
	int64_t int64;
	uint64_t uint64;
	int32_t int32;
	uint32_t uint32;
	int16_t int16;
	uint16_t uint16;
	int8_t int8;
	uint8_t uint8;
} number_t;

typedef enum {
	intmax_type_t,
	int64_type_t,
	int32_type_t,
	int16_type_t,
	int8_type_t,
	// unsigned must come after signed.
	// uintmax_type_t must come first.
	uintmax_type_t,
	uint64_type_t,
	uint32_type_t,
	uint16_type_t,
	uint8_type_t
} type_t;

static number_t strtonumber( const char *str, int base, const type_t type ) {
	errno = 0;
	number_t ret;
	char *endptr;
	if( type >= uintmax_type_t ) {
		ret.uintmax = strtoumax(str, &endptr, base);
		if( errno != 0 ) return ret;
		switch( type ) {
			case uintmax_type_t:
				break;
			case uint64_type_t:
				if( ret.uint64 != ret.uintmax ) errno = ERANGE;
				break;
			case uint32_type_t:
				if( ret.uint32 != ret.uintmax ) errno = ERANGE;
				break;
			case uint16_type_t:
				if( ret.uint16 != ret.uintmax ) errno = ERANGE;
				break;
			case uint8_type_t:
				if( ret.uint8 != ret.uintmax ) errno = ERANGE;
				break;
			default:
				efprintf(stderr, "Unknown type_t type\n");
				exit(EX_SOFTWARE);
		}
	} else {
		ret.intmax = strtoimax(str, &endptr, base);
		if( errno != 0 ) return ret;
		switch( type ) {
			case intmax_type_t:
				break;
			case int64_type_t:
				if( ret.int64 != ret.intmax ) errno = ERANGE;
				break;
			case int32_type_t:
				if( ret.int32 != ret.intmax ) errno = ERANGE;
				break;
			case int16_type_t:
				if( ret.int16 != ret.intmax ) errno = ERANGE;
				break;
			case int8_type_t:
				if( ret.int8 != ret.intmax ) errno = ERANGE;
				break;
			default:
				efprintf(stderr, "Unknown type_t type\n");
				exit(EX_SOFTWARE);
		}
	}
	return ret;
}

static number_t estrtonumber( const char *str, int base, const type_t type ) {
	number_t ret = strtonumber(str, base, type);
	if( errno != 0 ) {
		perror("strtonumber");
		exit(EX_DATAERR);
	}
	return ret;
}

static void help() {
	for( size_t i = 0; i < help_options_len; i++ ) {
		help_option_t *option = &help_options[i];
		eprintf("  -%c, --", option->longopt.val);
		const char *fmt = "%-22s";
		if( option->argname == NULL ) {
			eprintf(fmt, option->longopt.name);
		} else {
			size_t longopt_name_len = strlen(option->longopt.name), longopt_argname_len = strlen(option->argname),
				   longopt_str_len = 2 + longopt_name_len + longopt_argname_len;
			char longopt_str[longopt_str_len];
			longopt_str[0] = '\0';
			strcat(longopt_str, option->longopt.name);
			strcat(longopt_str, "=");
			strcat(longopt_str, option->argname);
			eprintf(fmt, longopt_str);
		}
		eprintf(" %s\n", option->helpstr);
	}
}

static void usage( FILE *stream ) {
	efprintf(stream, "Usage: %s [options] [format]\n", progname);
}

typedef struct {
	union {
		const char *constant;
		char *mutable;
	} str;
	size_t len;
	bool heap : 1;
	bool size_known : 1;
} component_t;

static size_t component_printf( component_t *component, const char *fmt, ... ) {
	va_list ap;
	va_start(ap, fmt);
	component->len = evasprintf(&component->str.mutable, fmt, ap);
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
		if( *cptr == '%' )
			ncomponents += 2;

	component_t components[ncomponents];
	bzero(components, sizeof(component_t) * ncomponents);
	component_t *next_component = components;
	(next_component++)->str.mutable = fmt;

	size_t size = 1;
	for( size_t i = 0; i < len; i++ ) {
		if( fmt[i] == '%' ) {
			fmt[i++] = '\0';
			switch( fmt[i] ) {
				case 't':
					(next_component++)->str.constant = stat->f_fstypename;
					break;
				case 'f':
					(next_component++)->str.constant = stat->f_mntfromname;
					break;
				case 'o':
					(next_component++)->str.constant = stat->f_mntonname;
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
					size += component_printf(next_component++, "%s", egetpwuid(stat->f_owner)->pw_name);
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
					(next_component++)->str.constant = "%";
					break;
				case '\0':
					efprintf(stderr, "Missing format specifier at end of format string\n");
					exit(EX_DATAERR);
				default:
					efprintf(stderr, "Unknown format specifier '%c'\n", fmt[i]);
					exit(EX_DATAERR);
			}
			fmt[i] = '\0';
			(next_component++)->str.mutable = &fmt[i + 1];
		}
	}

	for( size_t i = 0; i < ncomponents; i++ ) {
		if( !components[i].size_known ) {
			components[i].len = strlen(components[i].str.constant);
			size += components[i].len;
		}
	}

	char *ret = ecalloc(size, sizeof(char));
	char *ret_next = ret;
	for( size_t i = 0; i < ncomponents; i++ ) {
		size_t count = components[i].len * sizeof(char);
		memcpy(ret_next, components[i].str.constant, count);
		ret_next += count;
		if( components[i].heap ) free(components[i].str.mutable);
	}
	*ret_next = '\0';
	return ret;
}

int main( int argc, char *argv[] ) {
	progname = argv[0];
	char *fstypename = NULL, *mntfromname = NULL, *mntonname = NULL;
	bool has_bsize = false, has_iosize = false, has_blocks = false, has_bfree = false,
		 has_bavail = false, has_files = false, has_ffree = false, has_fsid = false,
		 has_owner = false, has_type = false, has_flags = false, has_fssubtype = false,
		 has_fsid0 = false, has_fsid1 = false;
	uint32_t bsize, type, flags, fssubtype;
	int32_t iosize;
	uint64_t blocks, bfree, bavail, files, ffree;
	fsid_t fsid = {
		.val = { 0, 0 }
	};
	uid_t owner;
	bool quiet = false;

	struct option longopts[help_options_len + 1];
	for( size_t i = 0; i < help_options_len; i++ )
		longopts[i] = help_options[i].longopt;
	bzero(&longopts[help_options_len], sizeof(struct option));

	int ch;
	while( (ch = getopt_long(argc, argv, "qht:f:o:B:I:b:F:a:n:e:S:T:U:O:g:s:", longopts, NULL)) != -1 ) {
		switch( ch ) {
			case 't':
				errno = 0;
				type = strtonumber(optarg, 10, uint32_type_t).uint32;
				if( errno == 0 ) {
					has_type = true;
				} else if( errno == EINVAL ) {
					has_type = false;
					fstypename = optarg;
				} else {
					perror("strtonumber");
					exit(EX_DATAERR);
				}
				break;
			case 'f':
				mntfromname = optarg;
				break;
			case 'o':
				mntonname = optarg;
				break;
			case 'B':
				has_bsize = true;
				bsize = estrtonumber(optarg, 10, uint32_type_t).uint32;
				break;
			case 'I':
				has_iosize = true;
				iosize = estrtonumber(optarg, 10, int32_type_t).int32;
				break;
			case 'b':
				has_blocks = true;
				blocks = estrtonumber(optarg, 10, uint64_type_t).uint64;
				break;
			case 'F':
				has_bfree = true;
				bfree = estrtonumber(optarg, 10, uint64_type_t).uint64;
				break;
			case 'a':
				has_bavail = true;
				bavail = estrtonumber(optarg, 10, uint64_type_t).uint64;
				break;
			case 'n':
				has_files = true;
				files = estrtonumber(optarg, 10, uint64_type_t).uint64;
				break;
			case 'e':
				has_ffree = true;
				ffree = estrtonumber(optarg, 10, uint64_type_t).uint64;
				break;
			case 'S':
				has_fsid0 = true;
				fsid.val[0] = estrtonumber(optarg, 10, int32_type_t).int32;
				break;
			case 'T':
				has_fsid1 = true;
				fsid.val[1] = estrtonumber(optarg, 10, int32_type_t).int32;
				break;
			case 'U':
				has_fsid = true;
				*((int64_t *) &fsid.val) = estrtonumber(optarg, 10, int64_type_t).int64;
				break;
			case 'O':
				has_owner = true;
				errno = 0;
				uintmax_t o;
				o = strtonumber(optarg, 10, uintmax_type_t).uintmax;
				if( errno == 0 ) {
					if( o != (uid_t) o ) {
						errno = ERANGE;
						perror("main");
						exit(EX_DATAERR);
					}
					owner = egetpwuid((uid_t) o)->pw_uid;
				} else if( errno == EINVAL ) {
					owner = egetpwnam(optarg)->pw_uid;
				} else {
					perror("strtonumber");
					exit(EX_DATAERR);
				}
				break;
			case 'g':
				has_flags = true;
				flags = estrtonumber(optarg, 10, uint32_type_t).uint32;
				break;
			case 's':
				has_fssubtype = true;
				fssubtype = estrtonumber(optarg, 10, uint32_type_t).uint32;
				break;
			case 'q':
				quiet = true;
				break;
			case 'h':
				usage(stdout);
				help();
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
		if( fstypename != NULL && strncmp(fstypename, mntbuf[i].f_fstypename, MFSTYPENAMELEN) != 0 ) continue;
		if( mntfromname != NULL && strncmp(mntfromname, mntbuf[i].f_mntfromname, MAXPATHLEN) != 0 ) continue;
		if( mntonname != NULL && strncmp(mntonname, mntbuf[i].f_mntonname, MAXPATHLEN) != 0 ) continue;
		if( has_bsize && bsize != mntbuf[i].f_bsize ) continue;
		if( has_iosize && iosize != mntbuf[i].f_iosize ) continue;
		if( has_blocks && blocks != mntbuf[i].f_blocks ) continue;
		if( has_bfree && bfree != mntbuf[i].f_bfree ) continue;
		if( has_bavail && bavail != mntbuf[i].f_bavail ) continue;
		if( has_files && files != mntbuf[i].f_files ) continue;
		if( has_ffree && ffree != mntbuf[i].f_ffree ) continue;
		if( has_fsid && *((int64_t *) &fsid.val) != *((int64_t *) &mntbuf[i].f_fsid) ) continue;
		if( has_owner && owner != mntbuf[i].f_owner ) continue;
		if( has_type && type != mntbuf[i].f_type ) continue;
		if( has_flags && flags != mntbuf[i].f_flags ) continue;
		if( has_fssubtype && fssubtype != mntbuf[i].f_fssubtype ) continue;
		if( has_fsid0 && fsid.val[0] != mntbuf[i].f_fsid.val[0] ) continue;
		if( has_fsid1 && fsid.val[1] != mntbuf[i].f_fsid.val[1] ) continue;
		if( !quiet ) {
			char *str = get_formatted_string(format, &mntbuf[i]);
			eprintf("%s\n", str);
			free(str);
		}
		retval = EX_OK;
	}
	return retval;
}
