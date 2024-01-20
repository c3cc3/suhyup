/*
 ** fq_lsof.c
 */
#ifndef OS_AIX
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_MAX 1024
#define CMD_DISPLAY_MAX 10

struct pid_info_t {
    pid_t pid;

    char cmdline[CMD_DISPLAY_MAX];

    char path[PATH_MAX];
    ssize_t parent_length;
};

void print_header()
{
    printf("%-9s %5s %10s %4s %9s %18s %9s %10s %s\n",
            "COMMAND",
            "PID",
            "USER",
            "FD",
            "TYPE",
            "DEVICE",
            "SIZE/OFF",
            "NODE",
            "NAME");
}

void print_type(char *type, struct pid_info_t* info)
{
    static ssize_t link_dest_size;
    static char link_dest[5112];

    strncat(info->path, type, sizeof(info->path)-1);

    if ((link_dest_size = readlink(info->path, link_dest, sizeof(link_dest)-1)) < 0) {
        if (errno == ENOENT)
            goto out;

        snprintf(link_dest, sizeof(link_dest)-1, "%s (readlink: %s)", info->path, strerror(errno));
    } else {
        link_dest[link_dest_size] = '\0';
    }

    // Things that are just the root filesystem are uninteresting (we already know)
    if (!strcmp(link_dest, "/"))
        goto out;

#ifdef _PRINT_CMD
    printf("%-9s %5d %10s %4s %9s %18s %9s %10s %s\n", info->cmdline, info->pid, "???", type,
            "???", "???", "???", "???", link_dest);
#endif

out:
    info->path[info->parent_length] = '\0';
}

// Prints out all file that have been memory mapped
void print_maps(struct pid_info_t* info)
{
    FILE *maps;
    char buffer[PATH_MAX + 100];

    size_t offset;
    int major, minor;
    char device[10];
    long int inode;
    char file[PATH_MAX];

    strncat(info->path, "maps", sizeof(info->path));

    maps = fopen(info->path, "r");
    if (!maps)
        goto out;

    while (fscanf(maps, "%*x-%*x %*s %zx %5s %ld %s\n", &offset, device, &inode,
            file) == 4) {
        // We don't care about non-file maps
        if (inode == 0 || !strcmp(device, "00:00"))
            continue;

#ifdef _PRINT_CMD
        printf("%-9s %5d %10s %4s %9s %18s %9zd %10ld %s\n", info->cmdline, info->pid, "???", "mem",
                "???", device, offset, inode, file);
#endif
    }

    fclose(maps);

out:
    info->path[info->parent_length] = '\0';
}

// Prints out all open file descriptors
int  print_fds(struct pid_info_t* info)
{
	int count_fds=0;
    static char* fd_path = "fd/";
    strncat(info->path, fd_path, sizeof(info->path));

    int previous_length = info->parent_length;
    info->parent_length += strlen(fd_path);

    DIR *dir = opendir(info->path);
    if (dir == NULL) {
        char msg[65536];

        snprintf(msg, sizeof(msg)-1, "%s (opendir: %s)", info->path, strerror(errno));


#ifdef _PRINT_CMD
        printf("%-9s %5d %10s %4s %9s %18s %9s %10s %s\n", info->cmdline, info->pid, "???", "FDS",
                "", "", "", "", msg);
#endif
        goto out;
    }

    struct dirent* de;
    while ((de = readdir(dir))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

		count_fds++;
        print_type(de->d_name, info);
    }
    closedir(dir);

out:
    info->parent_length = previous_length;
    info->path[info->parent_length] = '\0';

	return(count_fds);
}

int  lsof_dumpinfo(pid_t pid)
{
	int count_fds=0;
    int fd;
    struct pid_info_t info;
    info.pid = pid;

    snprintf(info.path, sizeof(info.path), "/proc/%d/", pid);

    info.parent_length = strlen(info.path);

    // Read the command line information; each argument is terminated with NULL.
    strncat(info.path, "cmdline", sizeof(info.path));
    fd = open(info.path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Couldn't read %s\n", info.path);
        return -1;
    }
    char cmdline[PATH_MAX];
    if (read(fd, cmdline, sizeof(cmdline)) < 0) {
        fprintf(stderr, "Error reading cmdline: %s: %s\n", info.path, strerror(errno));
        close(fd);
        return -2;
    }
    close(fd);
    info.path[info.parent_length] = '\0';

    // We only want the basename of the cmdline
    strncpy(info.cmdline, basename(cmdline), sizeof(info.cmdline));
    info.cmdline[sizeof(info.cmdline)-1] = '\0';

    // Read each of these symlinks
    print_type("cwd", &info);
    print_type("exe", &info);
    print_type("root", &info);

    count_fds = print_fds(&info);
    // print_maps(&info);

	return(count_fds);
}

int lsof_main(int argc, char *argv[])
{
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        fprintf(stderr, "Couldn't open /proc\n");
        return -1;
    }

#ifdef _PRINT_CMD
    print_header();
#endif

    struct dirent* de;
    while ((de = readdir(dir))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        // Only inspect directories that are PID numbers
        char* endptr;
        long int pid = strtol(de->d_name, &endptr, 10);
        if (*endptr != '\0')
            continue;

        lsof_dumpinfo(pid);
    }
    closedir(dir);

    return 0;
}

int find_pid_fds(long int my_pid)
{
	int count_fds=0;

    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        fprintf(stderr, "Couldn't open /proc\n");
        return -1;
    }

#ifdef _PRINT_CMD
    print_header();
#endif

    struct dirent* de;
    while ((de = readdir(dir))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        // Only inspect directories that are PID numbers
        char* endptr;
        long int pid = strtol(de->d_name, &endptr, 10);
        if (*endptr != '\0')
            continue;

		if( pid == my_pid ) {
			count_fds = lsof_dumpinfo(pid);
			break;
		}
    }
    closedir(dir);

    return count_fds;
}
#endif
