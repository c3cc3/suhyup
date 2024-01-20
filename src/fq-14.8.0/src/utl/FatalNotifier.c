#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>

static void             /* Display information from inotify_event structure */
displayInotifyEvent(char *path, struct inotify_event *i)
{
    printf("    wd =%2d; ", i->wd);
    if (i->cookie > 0)
        printf("cookie =%4d; ", i->cookie);

    printf("mask = ");
    if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
    if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
    if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
    if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
    if (i->mask & IN_CREATE)        printf("IN_CREATE ");
    if (i->mask & IN_DELETE)        printf("IN_DELETE ");
    if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
    if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
    if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
    if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
    if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
    if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
    if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
    if (i->mask & IN_OPEN)          printf("IN_OPEN ");
    if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
    if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
    printf("\n");

    if (i->len > 0) {
		char pathfile[128];
		FILE *fp;
		char buf[512];

		printf("        path = %s\n", path);
        printf("        name = %s\n", i->name);
		sprintf(pathfile, "%s/%s", path, i->name);
		fp = fopen(pathfile, "r");
		if(!fp) 
			return;
		
		while(fgets( buf, 512, fp)) {
			fprintf(stdout, "%s", buf);
		}
		fclose(fp);

		unlink(pathfile);
	}
}

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int
main(int argc, char *argv[])
{
    int inotifyFd, wd, j;
    char buf[BUF_LEN] __attribute__ ((aligned(8)));
    ssize_t numRead;
    char *p;
    struct inotify_event *event;

    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
		printf("Usage: %s path <ENTER>\n", argv[0]);
		printf("   ex) %s /tmp/queue_fatal <enter>\n", argv[0]);
		return 0;
	}

    inotifyFd = inotify_init();                 /* Create inotify instance */
    if (inotifyFd == -1) {
		printf("inotify_init() error.\n");
		return 0;
	}

    wd = inotify_add_watch(inotifyFd, argv[1], IN_ALL_EVENTS);
	if (wd == -1) {
		printf("inotify_add_watch() error.\n");
		return 0;
    }

    for (;;) {                                  /* Read events forever */
        numRead = read(inotifyFd, buf, BUF_LEN);
        if (numRead == 0) {
            printf("read() from inotify fd returned 0!\n");
			return 0;
		}
        if (numRead == -1) {
            printf("read fatal error.\n");
			return 0;
		}
        printf("Read %ld bytes from inotify fd\n", (long) numRead);

        /* Process all of the events in buffer returned by read() */
        for (p = buf; p < buf + numRead; ) {
            event = (struct inotify_event *) p;
            displayInotifyEvent(argv[1], event);

            p += sizeof(struct inotify_event) + event->len;
        }
    }
    exit(EXIT_SUCCESS);
}
