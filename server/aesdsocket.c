#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include "queue.h"

#define PORT "9000"
#define MAXBUFLEN (256 * 1024U)
#define TMPFILE "/var/tmp/aesdsocketdata"

typedef struct thread_args_s thread_args_t;
typedef struct slist_data_s slist_data_t;

struct thread_args_s
{
	int complete;
	int rv;
	int listenfd;
	struct sockaddr laddr;
};
struct slist_data_s
{
	pthread_t thread;
	thread_args_t threadargs;
	SLIST_ENTRY(slist_data_s)
	entries;
};
bool caughtsig = false;
pthread_mutex_t flock;
FILE *fp;

static void sig_handler(int signum)
{
	caughtsig = true;
}

static void time_thread(union sigval sv)
{
	char outstr[256];
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	if ((tmp = localtime(&t)) == NULL)
	{
		perror("aesdsocket: localtime");
	}
	if (strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp) == 0)
	{
		perror("aesdsocket: strftime");
	}
	if (pthread_mutex_lock(&flock) != 0)
	{
		perror("aesdsocket: pthread_mutex_lock");
	}
	fprintf(fp, "timestamp:%s\n", outstr);
	if (pthread_mutex_unlock(&flock) != 0)
	{
		perror("aesdsocket: pthread_mutex_unlock");
	}
}

static void *serve_thread(void *arg)
{
	char ipstr[INET_ADDRSTRLEN];
	char *recvbuf, *sendbuf;
	int rv;
	//int sockerr;
	size_t sendbuflen;
	//socklen_t sockerrlen;
	thread_args_t *targs = arg;

	targs->rv = 0;
	if (inet_ntop(targs->laddr.sa_family, targs->laddr.sa_data, ipstr, sizeof(ipstr)) == NULL)
	{
		perror("aesdsocket: inet_ntop");
		targs->rv = -1;
		return &targs->rv;
	}
	syslog(LOG_INFO, "Accepted connection from %s\n", ipstr);

	while (!caughtsig)
	{
		if ((recvbuf = malloc(MAXBUFLEN)) == NULL)
		{
			fprintf(stderr, "aesdsocket: malloc: Out of memory");
			targs->rv = -1;
			break;
		}
		if ((sendbuf = malloc(MAXBUFLEN)) == NULL)
		{
			fprintf(stderr, "aesdsocket: malloc: Out of memory");
			targs->rv = -1;
			break;
		}
		rv = recv(targs->listenfd, recvbuf, MAXBUFLEN, MSG_DONTWAIT);
		if (rv > 0)
		{
			recvbuf[rv] = '\0';
			if (pthread_mutex_lock(&flock) != 0)
			{
				perror("aesdsocket: pthread_mutex_lock");
			}
			if (fprintf(fp, "%s", recvbuf) < 0)
			{
				fprintf(stderr, "aesdsock: fprintf");
			}
			rewind(fp);
			while((sendbuflen = fread(sendbuf, sizeof(char), MAXBUFLEN, fp)) > 0) {
				if (send(targs->listenfd, sendbuf, sendbuflen, 0) == -1)
				{
					perror("aesdsock: send");
				}
			}
			if (pthread_mutex_unlock(&flock) != 0)
			{
				perror("aesdsocket: pthread_mutex_unlock");
			}
		}
		else if (rv == -1)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				targs->rv = -1;
				perror("aesdsock: recv");
			}
		}
		else
		{
			targs->rv = -1;
		}
		free(sendbuf);
		free(recvbuf);
		if (targs->rv < 0)
		{
			break;
		}
	}

	if (close(targs->listenfd) == -1)
	{
		perror("aesdsocket: close");
		targs->rv = -1;
	}
	syslog(LOG_INFO, "Closed connection from %s\n", ipstr);

	return &targs->rv;
}

int main(int argc, char *argv[])
{
	int opt, sockfd, listenfd, rv;
	struct addrinfo hints, *servinfo;
	struct sigaction sigact;
	struct sockaddr laddr;
	socklen_t laddrsz;
	pthread_t thread;
	bool daemon = false;
	int yes = 1;
	slist_data_t *datap = NULL;
	slist_data_t *tdatap = NULL;
	timer_t timer;
	struct sigevent sev;
	struct itimerspec its;

	while ((opt = getopt(argc, argv, "d")) != -1)
	{
		switch (opt)
		{
		case 'd':
			daemon = true;
			break;
		default:
			fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
			return -1;
		}
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "aesdsocket: getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}
	if ((sockfd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol)) == -1)
	{
		perror("aesdsocket: socket");
		return -1;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("aesdsocket: setsocketopt");
		return -1;
	}
	if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
	{
		perror("aesdsocket: bind");
		return -1;
	}

	freeaddrinfo(servinfo);

	if (daemon == true)
	{
		switch (fork())
		{
		case 0: /* Child process */
			syslog(LOG_INFO, "Running as daemon");
			break;
		case -1: /* Error */
			perror("aesdsocket: fork");
			return -1;
		default: /* Parent process */
			return 0;
		}
	}

	if (listen(sockfd, 16) == -1)
	{
		perror("aesdsocket: listen");
		return -1;
	}

	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = sig_handler;
	if (sigaction(SIGINT, &sigact, NULL) == -1)
	{
		perror("aesdsocket: sigaction");
		return -1;
	}
	if (sigaction(SIGTERM, &sigact, NULL) == -1)
	{
		perror("aesdsocket: sigaction");
		return -1;
	}

	if (pthread_mutex_init(&flock, NULL) != 0)
	{
		perror("aesdsocket: pthread_mutex_init");
		return -1;
	}

	if ((fp = fopen(TMPFILE, "w+")) == NULL)
	{
		perror("aesdsocket: fopen");
		return -1;
	}

	memset(&sev, 0, sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = &time_thread;
	if (timer_create(CLOCK_MONOTONIC, &sev, &timer) != 0)
	{
		perror("aesdsocket: timer_create");
		return -1;
	}
	its.it_value.tv_sec = 10;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 10;
	its.it_interval.tv_nsec = 0;
	if (timer_settime(timer, 0, &its, NULL) != 0)
	{
		perror("aesdsocket: timer_settime");
		return -1;
	}

	SLIST_HEAD(slisthead, slist_data_s)
	head;
	SLIST_INIT(&head);
	while (!caughtsig)
	{
		laddrsz = sizeof(struct sockaddr);
		if ((listenfd = accept(sockfd, &laddr, &laddrsz)) == -1)
		{
			perror("aesdsocket: accept");
			continue;
		}

		SLIST_FOREACH_SAFE(datap, &head, entries, tdatap)
		{
			if (datap->threadargs.complete)
			{
				if ((rv = pthread_join(datap->thread, NULL)) != 0)
				{
					fprintf(stderr, "aesdsocket: pthread_join: %s\n", strerror(rv));
				}
				SLIST_REMOVE(&head, datap, slist_data_s, entries);
				free(datap);
			}
		}

		datap = malloc(sizeof(slist_data_t));
		datap->threadargs.complete = false;
		datap->threadargs.listenfd = listenfd;
		datap->threadargs.laddr = laddr;
		if ((rv = pthread_create(&thread, NULL, serve_thread, &datap->threadargs)) != 0)
		{
			fprintf(stderr, "aesdsocket: pthread_create: %s\n", strerror(rv));
			free(datap);
			continue;
		}
		datap->thread = thread;
		SLIST_INSERT_HEAD(&head, datap, entries);
	}

	if (caughtsig)
	{
		syslog(LOG_INFO, "Caught signal, exiting");
	}

	rv = 0;
	while (!SLIST_EMPTY(&head))
	{
		datap = SLIST_FIRST(&head);
		if (pthread_join(datap->thread, NULL) != 0)
		{
			fprintf(stderr, "aesdsocket: pthread_join: %s\n", strerror(rv));
			rv = -1;
		}
		SLIST_REMOVE_HEAD(&head, entries);
		free(datap);
	}
	if (timer_delete(timer) != 0)
	{
		perror("aesdsocket: timer_delete");
		rv = -1;
	}
	if (fclose(fp) == EOF)
	{
		perror("aesdsocket: fclose");
		rv = -1;
	}
	if (pthread_mutex_destroy(&flock) != 0)
	{
		perror("aesdsocket: pthread_mutex_destroy");
		rv = -1;
	}
	if (shutdown(sockfd, SHUT_RDWR) == -1)
	{
		perror("aesdsocket: shutdown");
		rv = -1;
	}
	if (close(sockfd) == -1)
	{
		perror("aesdsocket: close");
		rv = -1;
	}
	if (access(TMPFILE, F_OK) == 0)
	{
		if (remove(TMPFILE) == -1)
		{
			perror("aesdsocket: remove");
			rv = -1;
		}
	}
	return rv;
}
