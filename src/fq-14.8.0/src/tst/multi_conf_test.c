#include <stdio.h>
#include <stdlib.h>
#include "fq_multi_config.h"

int main(int ac, char **av)
{
	SZ_toDefine_t toArg;

	FILE 	*fp=NULL;
	char	*server_listen_ip=NULL;
	char	*server_listen_port=NULL;
	char	*client_ip=NULL;
	char	*client_header_type=NULL;
	int		i_port;
	double  d_percent;

	if( ac != 2 ) {	
		printf("Usage: # %s [config file] <enter>\n", av[0]);
		exit(0);
	}

	/* 환경파일 구조체 초기화 */
	printf("Initializing test.\n");
	SZ_toInit(&toArg);

	/* 2.환경설정파일을 읽어서 구조체에 Setting 한다.  */
	printf("Loading test.\n");
	if(SZ_fromFile( &toArg, av[1]) <= 0) {
		perror("SZ_fromFile() error.");
		return(0);	
	}

	printf("Extracting test.\n");
	if( (server_listen_ip = SZ_toString( &toArg, "server.listen_ip")) == NULL) {
		printf("not found.(server.listen_ip)\n");
	}
	else {
		printf("found. value=[%s]\n", server_listen_ip);
	}

	printf("Extracting test(String).\n");
	if( (server_listen_port = SZ_toString( &toArg, "server.listen_port")) == NULL) {
		printf("not found.(server.listen_port)\n");
	}
	else {
		printf("found. value=[%s]\n", server_listen_port);
	}

	printf("Extracting test(Integer).\n");
	i_port = SZ_toInteger( &toArg, "server.listen_port");
	printf("listen_port found. value=[%d]\n", i_port);


	printf("Extracting test(Double).\n");
	d_percent = SZ_toDouble( &toArg, "server.percent");
	printf("percent found. value=[%5.2f]\n", d_percent);

	printf("Extracting test(Float).\n");
	d_percent = SZ_toFloat( &toArg, "server.percent");
	printf("percent found. value=[%f]\n", d_percent);

	printf("Defining test.\n");
	if ( SZ_toDefine( &toArg, "CLIENT-IP", "111.111.111.111") < 0 ) {
		perror("SZ_toDefine()\n");
		return(0);
	}
	printf("new Defined. CLIENT-IP=[111.111.111.111]\n");
	SZ_toCommit(&toArg);

	printf("Extracting test.\n");
	if( (client_ip = SZ_toString( &toArg, "CLIENT-IP")) == NULL) {
		printf("not found.(CLIENT-IP)\n");
	}
	else {
		printf("found. value=[%s]\n", client_ip);
	}

	printf("Extracting test.\n");
	if( ( client_header_type = SZ_toString( &toArg, "client.header_type")) == NULL) {
		printf("not found.(client.header_type)\n");
	}
	else {
		printf("found. value=[%s]\n", client_header_type);
	}

	printf("OK\n");
	return(0);
}
