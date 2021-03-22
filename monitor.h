int monitor_insert_directory(int fd, char *name, const char * path);
int monitor_remove_file(const char * path);
int monitor_remove_directory(int fd, const char * path);

#ifdef HAVE_INOTIFY
#ifdef _WIN32
DWORD WINAPI start_inotify(LPVOID lpParam);
#else
void *
start_inotify();
#endif
#endif
