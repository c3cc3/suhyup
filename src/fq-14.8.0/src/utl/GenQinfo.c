#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int ac, char **av)
{
	FILE *fq=NULL;
	char fname[128];
	char qpath[128];
	char file[512];
	char qname[16];
	char hostname[64];

	if(ac != 3) {
		printf("Usage: $ %s [Qpath] [Qname] <enter>\n", av[0]);
		exit(0);
	}

	sprintf(qpath, "%s", av[1]);
	sprintf(qname, "%s", av[2]);
	sprintf(fname, "%s.Q.info", av[2]);

	sprintf(file, "%s/%s", qpath, fname);
	gethostname(hostname, 64);

	fq=fopen(file, "w");
	if( fq == NULL) {
		printf("File open error.\n");
		exit(0);
	}

	fprintf(fq, "QPATH = \"%s\"\n", qpath);
	fprintf(fq, "QNAME = \"%s\"\n", qname);
	fprintf(fq, "MSGLEN = %d\n", 4096);
	fprintf(fq, "MMAPPING_NUM = %d\n", 100);
	fprintf(fq, "MULTI_NUM = %d\n", 1000);
	fprintf(fq, "DESC = \"%s\"\n", "Queue File");
	fprintf(fq, "XA_MODE_ON_OFF = \"%s\"\n", "OFF");
	fprintf(fq, "WAIT_MODE_ON_OFF = \"%s\"\n", "OFF");
	fprintf(fq, "MASTER_USE_FLAG = \"%s\"\n", "0");
	fprintf(fq, "MASTER_HOSTNAME = \"%s\"\n", hostname);

	fclose(fq);

	return(0);
}
