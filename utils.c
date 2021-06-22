/* MiniDLNA media server
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
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#else
#include <sys/param.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>

#include "minidlnatypes.h"
#include "upnpglobalvars.h"
#include "utils.h"
#include "log.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tchar.h>

 // Use your own error codes here
#define SUCCESS                     0L
#define FAILURE_NULL_ARGUMENT       1L
#define FAILURE_API_CALL            2L
#define FAILURE_INSUFFICIENT_BUFFER 3L

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

int gettimeofday(struct timeval* tv, struct timezone* tz) {
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;

	if(NULL != tv) {
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		tmpres /= 10;  /*convert into microseconds*/
		/*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if(NULL != tz) {
		if(!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

wchar_t* ToWide(char const* _FileName) {
	int len = MultiByteToWideChar(CP_UTF8, 0, _FileName, -1, NULL, 0);
	wchar_t* str = malloc(len * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, _FileName, -1, str, len);
	return str;
}

int my_access(char const* _FileName, int _AccessMode) {
	wchar_t* str = ToWide(_FileName);
	int ret = _waccess(str, _AccessMode & (F_OK | W_OK | R_OK));
	free(str);
	return ret;
}

int my_stat(char const* _FileName, struct my_stat* _Stat) {
	wchar_t* str = ToWide(_FileName);
	int ret = _wstat64(str, (struct _stat64*)_Stat);
	free(str);
	return ret;
}

FILE* my_fopen(const char* path, const char* mode) {
	wchar_t* str = ToWide(path);
	wchar_t modes[10];
	wchar_t* tmp = modes;
	for(int i = 0; i < 9 && *mode; i++) {
		*tmp = *mode;
		tmp++;
		mode++;
	}
	*tmp = 0;
	FILE* ret = _wfopen(str, modes);
	free(str);
	return ret;
}

static DWORD GetBasePathFromPathName(LPTSTR szPathName,
							  LPTSTR  szBasePath,
							  DWORD   dwBasePathSize) {
	TCHAR   szDrive[_MAX_DRIVE] = { 0 };
	TCHAR   szDir[_MAX_DIR] = { 0 };
	TCHAR   szFname[_MAX_FNAME] = { 0 };
	TCHAR   szExt[_MAX_EXT] = { 0 };
	size_t  PathLength;
	DWORD   dwReturnCode;

	// Parameter validation
	if(szPathName == NULL || szBasePath == NULL) {
		return FAILURE_NULL_ARGUMENT;
	}

	// Split the path into it's components
	dwReturnCode = _tsplitpath_s(szPathName, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT);
	if(dwReturnCode != 0) {
		_ftprintf(stderr, TEXT("Error splitting path. _tsplitpath_s returned %d.\n"), dwReturnCode);
		return FAILURE_API_CALL;
	}

	// Check that the provided buffer is large enough to store the results and a terminal null character
	PathLength = _tcslen(szDrive) + _tcslen(szDir);
	if((PathLength + sizeof(TCHAR)) > dwBasePathSize) {
		_ftprintf(stderr, TEXT("Insufficient buffer. Required %d. Provided: %d\n"), PathLength, dwBasePathSize);
		return FAILURE_INSUFFICIENT_BUFFER;
	}

	// Copy the szDrive and szDir into the provide buffer to form the basepath
	if((dwReturnCode = _tcscpy_s(szBasePath, dwBasePathSize, szDrive)) != 0) {
		_ftprintf(stderr, TEXT("Error copying string. _tcscpy_s returned %d\n"), dwReturnCode);
		return FAILURE_API_CALL;
	}
	if((dwReturnCode = _tcscat_s(szBasePath, dwBasePathSize, szDir)) != 0) {
		_ftprintf(stderr, TEXT("Error copying string. _tcscat_s returned %d\n"), dwReturnCode);
		return FAILURE_API_CALL;
	}
	szBasePath[PathLength > 0 ? PathLength - 1 : 0] = 0;
	return SUCCESS;
}

char* strsep(char** sp, char* sep) {
	char* p, * s;
	if(sp == NULL || *sp == NULL || **sp == '\0') return(NULL);
	s = *sp;
	p = s + strcspn(s, sep);
	if(*p != '\0') *p++ = '\0';
	*sp = p;
	return(s);
}


static int vasprintf(char** strp, const char* fmt, va_list ap) {
	// _vscprintf tells you how big the buffer needs to be
	int len = _vscprintf(fmt, ap);
	if(len == -1) {
		return -1;
	}
	size_t size = (size_t)len + 1;
	char* str = malloc(size);
	if(!str) {
		return -1;
	}
	// _vsprintf_s is the "secure" version of vsprintf
	int r = vsprintf_s(str, len + 1, fmt, ap);
	if(r == -1) {
		free(str);
		return -1;
	}
	*strp = str;
	return r;
}

char* dirname(char* path) {
	static char __declspec(thread) ret[MAX_PATH];
	if(path != ret)
		strcpy(ret, path);
	TCHAR base[MAX_PATH];
	base[0] = 0;
#ifdef UNICODE
	wchar_t* wpath = ToWide(path);
	GetBasePathFromPathName(wpath, base, MAX_PATH);
	free(wpath);
	size_t n = WideCharToMultiByte(CP_UTF8, 0, base, -1, ret, PATH_MAX, NULL, NULL);
	base[n] = 0;
#else
	GetBasePathFromPathName(path, base, MAX_PATH);
	memcpy(ret, base, MAX_PATH);
#endif

	return ret;
}
char* basename(char* path) {
	char* base = strrchr(path, '/');
	return base ? base + 1 : path;
}

int link(const char* path1, const char* path2) {
	wchar_t* wpath1 = ToWide(path1);
	wchar_t* wpath2 = ToWide(path2);
	BOOL ret = CreateHardLinkW(wpath2, wpath1, NULL);
	free(wpath2);
	free(wpath1);
	if(ret == 0) {
		if(GetLastError() == ERROR_PATH_NOT_FOUND) {
			errno = ENOENT;
		}
		return -1;
	}
	return 0;
}

int asprintf(char** strp, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int r = vasprintf(strp, fmt, ap);
	va_end(ap);
	return r;
}

char* strcasestr(const char* s, const char* find) {
	char c, sc;
	size_t len;
	if((c = *find++) != 0) {
		c = (char)tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if((sc = *s++) == 0)
					return (NULL);
			} while((char)tolower((unsigned char)sc) != c);
		} while(strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char*)s);
}

char* normalize_path(char* str) {
	char* current_pos = strchr(str, '\\');
	while(current_pos) {
		*current_pos = '/';
		current_pos = strchr(current_pos, '\\');
	}
	return str;
}
char* realpath(const char* x, char* y) {
	char* ret = _fullpath(y, x, PATH_MAX);
	normalize_path(ret);
	return ret;
}
#endif

int
xasprintf(char **strp, char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(strp, fmt, args);
	va_end(args);
	if( ret < 0 )
	{
		DPRINTF(E_WARN, L_GENERAL, "xasprintf: allocation failed\n");
		*strp = NULL;
	}
	return ret;
}

int
ends_with(const char * haystack, const char * needle)
{
	const char * end;
	int nlen = strlen(needle);
	int hlen = strlen(haystack);

	if( nlen > hlen )
		return 0;
	end = haystack + hlen - nlen;

	return (strcasecmp(end, needle) ? 0 : 1);
}

char *
trim(char *str)
{
	int i;
	int len;

	if (!str)
		return(NULL);

	len = strlen(str);
	for (i=len-1; i >= 0 && isspace(str[i]); i--)
	{
		str[i] = '\0';
		len--;
	}
	while (isspace(*str))
	{
		str++;
		len--;
	}

	if (str[0] == '"' && str[len-1] == '"')
	{
		str[0] = '\0';
		str[len-1] = '\0';
		str++;
	}

	return str;
}

/* Find the first occurrence of p in s, where s is terminated by t */
char *
strstrc(const char *s, const char *p, const char t)
{
	char *endptr;
	size_t slen, plen;

	endptr = strchr(s, t);
	if (!endptr)
		return strstr(s, p);

	plen = strlen(p);
	slen = endptr - s;
	while (slen >= plen)
	{
		if (*s == *p && strncmp(s+1, p+1, plen-1) == 0)
			return (char*)s;
		s++;
		slen--;
	}

	return NULL;
} 

char *
strcasestrc(const char *s, const char *p, const char t)
{
	char *endptr;
	size_t slen, plen;

	endptr = strchr(s, t);
	if (!endptr)
		return strcasestr(s, p);

	plen = strlen(p);
	slen = endptr - s;
	while (slen >= plen)
	{
		if (*s == *p && strncasecmp(s+1, p+1, plen-1) == 0)
			return (char*)s;
		s++;
		slen--;
	}

	return NULL;
} 

char *
modifyString(char *string, const char *before, const char *after, int noalloc)
{
	int oldlen, newlen, chgcnt = 0;
	char *s, *p;

	/* If there is no match, just return */
	s = strstr(string, before);
	if (!s)
		return string;

	oldlen = strlen(before);
	newlen = strlen(after);
	if (newlen > oldlen)
	{
		if (noalloc)
			return string;

		while ((p = strstr(s, before)))
		{
			chgcnt++;
			s = p + oldlen;
		}
		s = realloc(string, strlen(string)+((newlen-oldlen)*chgcnt)+1);
		/* If we failed to realloc, return the original alloc'd string */
		if( s )
			string = s;
		else
			return string;
	}

	s = string;
	while (s)
	{
		p = strstr(s, before);
		if (!p)
			return string;
		memmove(p + newlen, p + oldlen, strlen(p + oldlen) + 1);
		memcpy(p, after, newlen);
		s = p + newlen;
	}

	return string;
}

char *
unescape_tag(const char *tag, int force_alloc)
{
	char *esc_tag = NULL;

	if (strchr(tag, '&') &&
	    (strstr(tag, "&amp;") || strstr(tag, "&lt;") || strstr(tag, "&gt;") ||
	     strstr(tag, "&quot;") || strstr(tag, "&apos;")))
	{
		esc_tag = strdup(tag);
		esc_tag = modifyString(esc_tag, "&amp;", "&", 1);
		esc_tag = modifyString(esc_tag, "&lt;", "<", 1);
		esc_tag = modifyString(esc_tag, "&gt;", ">", 1);
		esc_tag = modifyString(esc_tag, "&quot;", "\"", 1);
		esc_tag = modifyString(esc_tag, "&apos;", "'", 1);
	}
	else if( force_alloc )
		esc_tag = strdup(tag);

	return esc_tag;
}

char *
escape_tag(const char *tag, int force_alloc)
{
	char *esc_tag = NULL;

	if( strchr(tag, '&') || strchr(tag, '<') || strchr(tag, '>') || strchr(tag, '"') )
	{
		esc_tag = strdup(tag);
		esc_tag = modifyString(esc_tag, "&", "&amp;amp;", 0);
		esc_tag = modifyString(esc_tag, "<", "&amp;lt;", 0);
		esc_tag = modifyString(esc_tag, ">", "&amp;gt;", 0);
		esc_tag = modifyString(esc_tag, "\"", "&amp;quot;", 0);
	}
	else if( force_alloc )
		esc_tag = strdup(tag);

	return esc_tag;
}

char *
duration_str(int msec)
{
	char *str;

	xasprintf(&str, "%d:%02d:%02d.%03d",
			(msec / 3600000),
			(msec / 60000 % 60),
			(msec / 1000 % 60),
			(msec % 1000));

	return str;
}

char *
strip_ext(char *name)
{
	char *period;

	if (!name)
		return NULL;
	period = strrchr(name, '.');
	if (period)
		*period = '\0';

	return period;
}

/* Code basically stolen from busybox */
int
make_dir(char * path, mode_t mode)
{
	char * s = path;
	char c;
	struct stat st;
	(void)st;

	do {
		c = '\0';

		/* Before we do anything, skip leading /'s, so we don't bother
		 * trying to create /. */
		while (*s == '/')
			++s;

		/* Bypass leading non-'/'s and then subsequent '/'s. */
		while (*s) {
			if (*s == '/' || *s == '\\') {
				do {
					++s;
				} while (*s == '/' || *s == '\\');
				c = *s;     /* Save the current char */
				*s = '\0';     /* and replace it with nul. */
				break;
			}
			++s;
		}
#ifdef _WIN32
#ifdef UNICODE
		wchar_t* wpath = ToWide(path);
		int failure = !CreateDirectory(wpath, NULL);
		free(wpath);
		if(failure) {
#else
		if(!CreateDirectory(path, NULL)) {
#endif
			DWORD error = GetLastError();
			if(error != ERROR_ALREADY_EXISTS && error != ERROR_ACCESS_DENIED) {
				DPRINTF(E_WARN, L_GENERAL, "make_dir: cannot create directory '%s'\n", path);
				if(c)
					*s = c;
				return -1;
			}
		}
#else
		if (mkdir(path, mode) < 0) {
			/* If we failed for any other reason than the directory
			 * already exists, output a diagnostic and return -1.*/
#ifndef __CYGWIN__
			if ((errno != EEXIST && errno != EISDIR)
#else
			if ((errno != EEXIST && errno != EISDIR && errno != EACCES)
#endif // __CYGWIN__
			    || (stat(path, &st) < 0 || !S_ISDIR(st.st_mode))) {
				DPRINTF(E_WARN, L_GENERAL, "make_dir: cannot create directory '%s'\n", path);
				if (c)
					*s = c;
				return -1;
			}
		}
#endif
		if (!c)
			return 0;

		/* Remove any inserted nul from the path. */
		*s = c;

	} while (1);
}

/* Simple, efficient hash function from Daniel J. Bernstein */
unsigned int
DJBHash(uint8_t *data, int len)
{
	unsigned int hash = 5381;
	int i = 0;

	for(i = 0; i < len; data++, i++)
	{
		hash = ((hash << 5) + hash) + (*data);
	}

	return hash;
}

const char *
mime_to_ext(const char * mime)
{
	switch( *mime )
	{
		/* Audio extensions */
		case 'a':
			if( strcmp(mime+6, "mpeg") == 0 )
				return "mp3";
			else if( strcmp(mime+6, "mp4") == 0 )
				return "m4a";
			else if( strcmp(mime+6, "x-ms-wma") == 0 )
				return "wma";
			else if( strcmp(mime+6, "x-flac") == 0 )
				return "flac";
			else if( strcmp(mime+6, "flac") == 0 )
				return "flac";
			else if( strcmp(mime+6, "x-wav") == 0 )
				return "wav";
			else if( strncmp(mime+6, "L16", 3) == 0 )
				return "pcm";
			else if( strcmp(mime+6, "3gpp") == 0 )
				return "3gp";
			else if( strcmp(mime, "application/ogg") == 0 )
				return "ogg";
			break;
		case 'v':
			if( strcmp(mime+6, "avi") == 0 )
				return "avi";
			else if( strcmp(mime+6, "divx") == 0 )
				return "avi";
			else if( strcmp(mime+6, "x-msvideo") == 0 )
				return "avi";
			else if( strcmp(mime+6, "mpeg") == 0 )
				return "mpg";
			else if( strcmp(mime+6, "mp4") == 0 )
				return "mp4";
			else if( strcmp(mime+6, "x-ms-wmv") == 0 )
				return "wmv";
			else if( strcmp(mime+6, "x-matroska") == 0 )
				return "mkv";
			else if( strcmp(mime+6, "x-mkv") == 0 )
				return "mkv";
			else if( strcmp(mime+6, "x-flv") == 0 )
				return "flv";
			else if( strcmp(mime+6, "vnd.dlna.mpeg-tts") == 0 )
				return "mpg";
			else if( strcmp(mime+6, "quicktime") == 0 )
				return "mov";
			else if( strcmp(mime+6, "3gpp") == 0 )
				return "3gp";
			else if( strncmp(mime+6, "x-tivo-mpeg", 11) == 0 )
				return "TiVo";
			break;
		case 'i':
			if( strcmp(mime+6, "jpeg") == 0 )
				return "jpg";
			else if( strcmp(mime+6, "png") == 0 )
				return "png";
			break;
		default:
			break;
	}
	return "dat";
}

int
is_video(const char * file)
{
	return (ends_with(file, ".mpg") || ends_with(file, ".mpeg")  ||
		ends_with(file, ".avi") || ends_with(file, ".divx")  ||
		ends_with(file, ".asf") || ends_with(file, ".wmv")   ||
		ends_with(file, ".mp4") || ends_with(file, ".m4v")   ||
		ends_with(file, ".mts") || ends_with(file, ".m2ts")  ||
		ends_with(file, ".m2t") || ends_with(file, ".mkv")   ||
		ends_with(file, ".vob") || ends_with(file, ".ts")    ||
		ends_with(file, ".flv") || ends_with(file, ".xvid")  ||
#ifdef TIVO_SUPPORT
		ends_with(file, ".TiVo") ||
#endif
		ends_with(file, ".mov") || ends_with(file, ".3gp"));
}

int
is_audio(const char * file)
{
	return (ends_with(file, ".mp3") || ends_with(file, ".flac") ||
		ends_with(file, ".wma") || ends_with(file, ".asf")  ||
		ends_with(file, ".fla") || ends_with(file, ".flc")  ||
		ends_with(file, ".m4a") || ends_with(file, ".aac")  ||
		ends_with(file, ".mp4") || ends_with(file, ".m4p")  ||
		ends_with(file, ".wav") || ends_with(file, ".ogg")  ||
		ends_with(file, ".pcm") || ends_with(file, ".3gp"));
}

int
is_image(const char * file)
{
	return (ends_with(file, ".jpg") || ends_with(file, ".jpeg"));
}

int
is_playlist(const char * file)
{
	return (ends_with(file, ".m3u") || ends_with(file, ".pls"));
}

int
is_caption(const char * file)
{
	return (ends_with(file, ".srt") || ends_with(file, ".smi"));
}

media_types
get_media_type(const char *file)
{
	const char *ext = strrchr(file, '.');
	if (!ext)
		return NO_MEDIA;
	if (is_image(ext))
		return TYPE_IMAGE;
	if (is_video(ext))
		return TYPE_VIDEO;
	if (is_audio(ext))
		return TYPE_AUDIO;
	if (is_playlist(ext))
		return TYPE_PLAYLIST;
	if (is_caption(ext))
		return TYPE_CAPTION;
	if (is_nfo(ext))
		return TYPE_NFO;
	return NO_MEDIA;
}

int
is_album_art(const char * name)
{
	struct album_art_name_s * album_art_name;

	/* Check if this file name matches one of the default album art names */
	for( album_art_name = album_art_names; album_art_name; album_art_name = album_art_name->next )
	{
		if( album_art_name->wildcard )
		{
			if( strncmp(album_art_name->name, name, strlen(album_art_name->name)) == 0 )
				break;
		}
		else
		{
			if( strcmp(album_art_name->name, name) == 0 )
				break;
		}
	}

	return (album_art_name ? 1 : 0);
}

int
resolve_unknown_type(const char * path, media_types dir_type)
{
	struct my_stat entry;
	enum file_types type = TYPE_UNKNOWN;

#ifndef _WIN32
	char str_buf[PATH_MAX];
	ssize_t len;
	if( lstat(path, &entry) == 0 )
	{
		if( S_ISLNK(entry.st_mode) )
		{
			if( (len = readlink(path, str_buf, PATH_MAX-1)) > 0 )
			{
				str_buf[len] = '\0';
				//DEBUG DPRINTF(E_DEBUG, L_GENERAL, "Checking for recursive symbolic link: %s (%s)\n", path, str_buf);
				if( strncmp(path, str_buf, strlen(str_buf)) == 0 )
				{
					DPRINTF(E_DEBUG, L_GENERAL, "Ignoring recursive symbolic link: %s (%s)\n", path, str_buf);
					return type;
				}
			}
			my_stat(path, &entry);
		}
#else
	if(my_stat(path, &entry) == 0) {
#endif
		if( S_ISDIR(entry.st_mode) )
		{
			type = TYPE_DIR;
		}
		else if( S_ISREG(entry.st_mode) )
		{
			media_types mtype = get_media_type(path);
			if (dir_type & mtype)
				type = TYPE_FILE;
		}
	}
	return type;
}

media_types
valid_media_types(const char *path)
{
	struct media_dir_s *media_dir;

	for (media_dir = media_dirs; media_dir; media_dir = media_dir->next)
	{
		if (strncmp(path, media_dir->path, strlen(media_dir->path)) == 0)
			return media_dir->types;
	}

	return ALL_MEDIA;
}
