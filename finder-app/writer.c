#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	char *writefile, *writestr;
	FILE *fp;
	int rc = 0;

	openlog(NULL, 0, LOG_USER);
	
	if (argc != 3)
	{
		syslog(LOG_DEBUG, "Invalid number of arguments: %d", argc-1);
		exit(1);
	}

	writefile=argv[1];
	writestr=argv[2];

	fp = fopen(writefile, "w");
	if(fp == NULL)
	{
		syslog(LOG_DEBUG, "%s", strerror(errno));
		exit(rc);
	}
	if(fprintf(fp, "%s", writestr) < 0)
	{
		syslog(LOG_DEBUG, "Write error"); 
		rc = 1;
	}	
	if(fclose(fp) != 0)
	{
		syslog(LOG_DEBUG, "%s", strerror(errno));
		exit(rc);
	}

	return rc;
}
