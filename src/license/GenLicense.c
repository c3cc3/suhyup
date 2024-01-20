/*************************************************************************/
/*  Author: Gwisang.Choi@gamil.com										 */
/*  Software license file generator                         			 */
/*  (C)Copyright 2010 Entrust Gwisang.choi                               */
/*************************************************************************/
// GenLicense.c
#include <stdio.h>
#include <fcntl.h>
#include "fq_common.h"

/**************************************************************************/
/*  unix_getnowtime                                                       */
/*  return current second XX.xxxxxx                                       */
/**************************************************************************/
static double fq_unix_getnowtime(char *timebuff)
{
	struct timeval	timelong;
	struct tm	*timest, date1;

	gettimeofday(&timelong, NULL);
	timest = (struct tm *)localtime_r(&timelong.tv_sec, &date1);
	if (timebuff != NULL)
	{
		sprintf(timebuff, "%04d%02d%02d%02d%02d%02d",
			timest->tm_year+1900, timest->tm_mon+1, timest->tm_mday, 
			timest->tm_hour, timest->tm_min, timest->tm_sec);
	}
	return ((double)timelong.tv_sec +
			(double)timelong.tv_usec/1000000.0);
}

int main(argc, argv)
int		argc;
char	**argv;
{
	int	i, hostlen, fd;
	char	tmp[32], hostname[32], datebuff[64], license[32];
	char	filename[256];
	char	password[32];

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	if (argc <= 2)
	{
		printf("Usage:$ %s [normal] [node_name that a value of (uname -n)]\n", argv[0]);
		printf("      $ %s normal hostname <ENTER>\n", argv[0]);
		printf("      $ %s normal test <ENTER>\n", argv[0]);
		exit(0);
	}

	getprompt("password:", password, 16);

	if( strcmp(password, "cks1485") != 0 ) {
		fprintf(stderr, "Sorry!..goodbye..\n");
		exit(0);
	}

	if (memcmp(argv[2], "test", 4) == 0)
	{
		sprintf(hostname, "test__%10.0f", fq_unix_getnowtime(datebuff));
		sprintf(filename, "%s", "./trial");
	}
	else
	{
		strcpy(hostname, argv[2]);
		hostlen = strlen(hostname);
		sprintf(filename, "./%s.license", hostname);
	}
	hostlen = strlen(hostname);

	if (memcmp(argv[1], "light", 5) == 0) memcpy(tmp, "<L>", 3);
	else if (memcmp(argv[1], "normal", 6) == 0) memcpy(tmp, "<N>", 3);

	for (i = 0; i < hostlen; i++) tmp[i+3] = hostname[i];

	for (i = hostlen; i < 28; i++)
	{
		tmp[i+3] = ' ';
	}
	tmp[28] = 0;

	printf("License file([%s]) will be allowed.\n", tmp);
	printf("................press any key.............\n");
	getchar();

	for (i = 0; i < 28; i++) {
		license[i] = tmp[i] - 31;
	}

	license[4] = tmp[4] - 48;
	license[7] = tmp[7] - 35;
	license[9] = tmp[9] - 42;
	license[10] = tmp[10] - 40;
	license[13] = tmp[13] - 38;

	fd = open(filename, O_WRONLY|O_CREAT, 0644);
	write(fd, license, 28);
	close(fd);

	return(EXIT_SUCCESS);
}
