/*
 * one line to give the program's name and an idea of what it does.
 * Copyright (C) yyyy  name of author
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/sendfile.h>

static const int min = 5;
static const char *delimiter = "/";

static char
*get_sub_path(const size_t path_depth, const char *path_name)
{
	size_t len, count;
	char *tmp, *buff, *token;

	len = 0;
	count = path_depth;
	tmp = strdup(path_name);

	if (tmp == NULL) return NULL;

	token = strtok(tmp, delimiter);

	count--;
	len += strlen(token);

	while (count--) {
		token = strtok(NULL, delimiter);
		len += strlen(token);
	}

	len += path_depth;

	buff = malloc(sizeof(char) * len);

	if (buff == NULL) return NULL;

	memcpy(buff, path_name, len);
	*(buff + len) = '\0';

	free(tmp);

	return buff;
}

static int
get_path_depth(const char *path_name)
{
	size_t depth;
	char *tmp, *token;

	depth = 0;
	tmp = strdup(path_name);

	if (!tmp) return -1;

	token = strtok(tmp, delimiter);

	while (token) {
		depth++;
		token = strtok(NULL, delimiter);
	}

	free(tmp);

	return depth;
}

static void
*unlink_routine(void *args)
{
	ssize_t num;
        int in_fd, out_fd, res;
        struct stat f_stat;

        in_fd = open((const char *) args, O_RDONLY);

        if (in_fd == -1) {
                pthread_exit(NULL);
        }

        res = fstat(in_fd, &f_stat);

        if (res == -1) {
                perror("fstat");
                pthread_exit(NULL);
        }
        
        res = unlink((const char *) args);

        if (res == -1) {
                perror("unlink");
                pthread_exit(NULL);
        }

        sleep(10);

        out_fd = open((const char *) args, O_WRONLY | O_CREAT | O_EXCL, \
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (out_fd == -1) {
                perror("open");
                pthread_exit(NULL);
        }

        num = sendfile(out_fd, in_fd, NULL, f_stat.st_size);

        if (num == -1) {
                perror("sendfile");
                pthread_exit(NULL);
        }

        close(in_fd);
        close(out_fd);
        pthread_exit(NULL);
}

int
main(int argc, char **argv)
{
	int fd, wd;
	size_t depth;
	char *watched;
	char *path_name;

	if (argc < 2) {
		printf("Invalid number of command line options\n");
		exit(EXIT_FAILURE);
	}

	path_name = realpath(argv[1], NULL);

	if (!path_name) {
		perror("realpath");
		exit(EXIT_FAILURE);	
	}
	
	depth = get_path_depth(path_name);

	if (depth == -1) {
		printf("Error occurred while calculating path depth\n");
		exit(EXIT_FAILURE);
	}

	if (depth <= min) {
		printf("Place file lower in file system hierarchy to avoid \
			detection\n");
		exit(EXIT_FAILURE);
	}

	fd = inotify_init();

	if (fd == -1) {
		perror("inotify_init");
		exit(EXIT_FAILURE);
	}

	watched = get_sub_path((depth - min), path_name);

	if (watched == NULL) {
		printf("error occurred\n");
		exit(EXIT_FAILURE);
	}

	wd = inotify_add_watch(fd, watched, IN_OPEN);

	if (wd == -1) {
		perror("inotify_add_watch");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		int res;
                ssize_t num;
                pthread_t thread;
                char buff[4096] __attribute__ ((aligned(__alignof__ \
                                (struct inotify_event))));

                num = read(fd, buff, sizeof buff);

                if (num == -1 && errno != EAGAIN) {
                        perror("read");
                        exit(EXIT_FAILURE);
                }

                res = pthread_create(&thread, NULL, unlink_routine, path_name);

                if (res != 0) {
                        errno = res;
                        perror("pthread_create");
                        exit(EXIT_FAILURE);
                }
	}

	free(watched);
	free(path_name);

	exit(EXIT_SUCCESS);
}
