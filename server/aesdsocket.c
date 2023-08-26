#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>

#define PORT		"9000"
#define MAXBUFLEN 	(1024*1024U)
#define TMPFILE		"/var/tmp/aesdsocketdata"	

bool caughtsig = false;

static void sig_handler(int signum)
{
	caughtsig = true;
}

int main(int argc, char *argv[])
{
	int opt, sockfd, listenfd, rv;
	struct addrinfo hints, *servinfo;
	struct sigaction sigact;
	struct sockaddr laddr;
	socklen_t laddrsz; 
	char ipstr[INET_ADDRSTRLEN];
	char *recvbuf, *sendbuf, *ptr;
	size_t sendbuflen;
	FILE *fp;
	bool daemon = false;
	int yes = 1;

	while((opt = getopt(argc, argv, "d")) != -1)
	{
		switch(opt) 
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
	if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "aesdsocket: getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	if((sockfd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol)) == -1)
	{
		perror("aesdsocket: socket");
		return -1;
	}
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("aesdsocket: setsocketopt");
		return -1;
	}

	if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
	{
		perror("aesdsocket: bind");
		return -1;
	}

	freeaddrinfo(servinfo);

	if(daemon == true)
	{
		switch(fork())
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

	if(listen(sockfd, 16) == -1)
	{
		perror("aesdsocket: listen");
		return -1;
	}	

	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = sig_handler;
	if(sigaction(SIGINT, &sigact, NULL) == -1)
	{
		perror("aesdsocket: sigaction");
		return -1;
	}
	if(sigaction(SIGTERM, &sigact, NULL) == -1)
	{
		perror("aesdsocket: sigaction");
		return -1;
	}	

	while(!caughtsig)
	{
		laddrsz = sizeof(struct sockaddr);
		if((listenfd = accept(sockfd, &laddr, &laddrsz)) == -1)
		{
			perror("aesdsocket: accept");
			continue;
		}
		rv = 0;
		if(inet_ntop(laddr.sa_family, laddr.sa_data, ipstr, sizeof(ipstr)) == NULL)
		{
			perror("aesdsocket: inet_ntop");
			rv = -1;
			break;
		}
		syslog(LOG_INFO, "Accepted connection from %s\n", ipstr);

		if((fp = fopen(TMPFILE, "a+")) == NULL)
		{
			perror("aesdsocket: fopen");
			rv = -1;
			break;
		}
		while(1)
		{	 	
			if((recvbuf = malloc(MAXBUFLEN)) == NULL)
			{	
				fprintf(stderr, "aesdsocket: malloc: Out of memory");
				rv = -1;
				break;
			}
			rv = recv(listenfd, recvbuf, MAXBUFLEN, 0);
			if(rv == -1)
			{
				perror("aesdsock: recv");
			}
			else if(rv > 0)
			{
				recvbuf[rv] = '\0';
				for (ptr = strtok(recvbuf,"\n"); ptr != NULL; ptr = strtok(NULL, " "))
				{
  					if(fprintf(fp, "%s\n", ptr) < 0)
					{
						fprintf(stderr, "aesdsock: fprintf");
					}
				}
				rewind(fp);
				while(1)
				{
					sendbuf = NULL;
					sendbuflen = 0;
					if(getline(&sendbuf, &sendbuflen, fp) != -1)
					{
						if(send(listenfd, sendbuf, strlen(sendbuf), 0) == -1)
						{
							perror("aesdsock: send");
						}
						free(sendbuf);
					}
					else
					{
						free(sendbuf);
						break;
					}					
				}
			}
			free(recvbuf);
			if (rv <= 0)
			{
				break;
			}
		}
	
		if(fclose(fp) == EOF)
		{
			perror("aesdsocket: fclose");
			rv = -1;
			break;
		}	
		if(close(listenfd) == -1)
		{
			perror("aesdsocket: close");
			rv = -1;
			break;
		}			
		syslog(LOG_INFO, "Closed connection from %s\n", ipstr);
	}

	if(caughtsig)
	{
		syslog(LOG_INFO, "Caught signal, exiting");
	}

	if(shutdown(sockfd, SHUT_RDWR) == -1)
	{
		perror("aesdsocket: shutdown");
		rv = -1;
	}
	if(close(sockfd) == -1)
	{
		perror("aesdsocket: close");
		rv = -1;
	}
	if(access(TMPFILE, F_OK) == 0)
	{
		if(remove(TMPFILE) == -1)
		{
			perror("aesdsocket: remove");	
			rv = -1;
		}
	}
	return rv;
}
