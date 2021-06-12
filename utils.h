/* Utility functions
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2017  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>
#ifdef _WIN32
#include <stdint.h>
#define strcasecmp(x,y) _stricmp(x,y)
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#define strtok_r strtok_s
#define __attribute__(...)
char* dirname(char* path);
char* basename(char* path);
char* strsep(char** sp, char* sep);
int asprintf(char** strp, const char* fmt, ...);
char* strcasestr(const char* s, const char* find);
char* normalize_path(char* str);
char* realpath(const char* x, char* y);
#define inet_aton(x, y) inet_pton(AF_INET, x,y)
#define mode_t int
#define S_IRWXU 0
#define S_IRGRP 0
#define S_IXGRP 0
#define S_IROTH 0
#define S_IXOTH 0
#define S_ISVTX 0
#define S_IRWXG 0
#define S_IRWXO 0
#define MIN min
#define MAXPATHLEN MAX_PATH
#define PATH_MAX MAX_PATH
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
int link(const char* path1, const char* path2);

struct timezone {
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};
int gettimeofday(struct timeval* tv, struct timezone* tz);
int my_access(char const* _FileName, int _AccessMode);
#include "dirent.h"
struct my_stat {
	_dev_t         st_dev;
	_ino_t         st_ino;
	unsigned short st_mode;
	short          st_nlink;
	short          st_uid;
	short          st_gid;
	_dev_t         st_rdev;
	__int64        st_size;
	__time64_t     st_atime;
	__time64_t     st_mtime;
	__time64_t     st_ctime;
};
int my_stat(char const* _FileName, struct my_stat* _Stat);
FILE* my_fopen(const char* path, const char* mode);
typedef int64_t my_off_t;
#define ftello _ftelli64
#define fseeko _fseeki64
#else
#include <dirent.h>
#include <sys/param.h>
#define my_access access
#define my_stat stat
#define my_fopen fopen
typedef off_t my_off_t;
#endif

#include "minidlnatypes.h"

/* String functions */
/* We really want this one inlined, since it has a major performance impact */
static inline int
__attribute__((__format__ (__printf__, 2, 3)))
strcatf(struct string_s *str, const char *fmt, ...)
{
	int ret;
	int size;
	va_list ap;

	if (str->off >= str->size)
		return 0;

	va_start(ap, fmt);
	size = str->size - str->off;
	ret = vsnprintf(str->data + str->off, size, fmt, ap);
	str->off += MIN(ret, size);
	va_end(ap);

	return ret;
}
static inline void strncpyt(char *dst, const char *src, size_t len)
{
	strncpy(dst, src, len);
	dst[len-1] = '\0';
}
static inline int is_reg(const struct dirent *d)
{
#if HAVE_STRUCT_DIRENT_D_TYPE
	return (d->d_type == DT_REG);
#else
	return -1;
#endif
}
static inline int is_dir(const struct dirent *d)
{
#if HAVE_STRUCT_DIRENT_D_TYPE
	return (d->d_type == DT_DIR);
#else
	return -1;
#endif
}
int xasprintf(char **strp, char *fmt, ...) __attribute__((__format__ (__printf__, 2, 3)));
int ends_with(const char * haystack, const char * needle);
char *trim(char *str);
char *strstrc(const char *s, const char *p, const char t);
char *strcasestrc(const char *s, const char *p, const char t);
char *modifyString(char *string, const char *before, const char *after, int noalloc);
char *escape_tag(const char *tag, int force_alloc);
char *unescape_tag(const char *tag, int force_alloc);
char *duration_str(int msec);
char *strip_ext(char *name);

/* Metadata functions */
int is_video(const char * file);
int is_audio(const char * file);
int is_image(const char * file);
int is_playlist(const char * file);
int is_caption(const char * file);
#define is_nfo(file) ends_with(file, ".nfo")
media_types get_media_type(const char *file);
media_types valid_media_types(const char *path);

int is_album_art(const char * name);
int resolve_unknown_type(const char * path, media_types dir_type);
const char *mime_to_ext(const char * mime);

/* Others */
int make_dir(char * path, mode_t mode);
unsigned int DJBHash(uint8_t *data, int len);

#endif
