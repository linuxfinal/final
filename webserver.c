#include "wrapper.h"

void *process_trans(void *vargp);
void read_requesthdrs(rio_t *rp);
int is_static(char *uri);
void parse_static_uri(char *uri,char *filename);
void parse_dynamic_uri(char *uri,char *filename,char *cgiargs);
void feed_static(int fd,char *filename,int filesize);
void get_filetype(char *filename,char *filetype);
void feed_dynamic(int fd,char *filename,char *cgiargs);
void error_request(int fd,char *cause,char *errnum,char *shortmsg,char *description);

int main(int argc,char **argv)
{
	int listen_sock,*conn_sock,port,clientlen;
	pthread_t tid;
	struct sockaddr_in clientaddr;
	if(argc!=2){
		fprintf(stderr,"usage: %s<port>\n",argv[0]);
		exit(1);
	}
	port=atoi(argv[1]);

	listen_sock = open_listen_sock(port);
	while(1){
		clientlen = sizeof(clientaddr);
		conn_sock = malloc(sizeof(int));
		*conn_sock = accept(listen_sock,(SA *)&clientaddr,&clientlen);
		pthread_create(&tid,NULL,process_trans,conn_sock);
	}
}

void *process_trans(void *vargp)
{
	int fd = *((int*)vargp);
	int static_flag;
	struct stat sbuf;
	char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
	char filename[MAXLINE],cgiargs[MAXLINE];
	rio_t rio;
	
	pthread_detach(pthread_self());
	free(vargp);
	rio_readinitb(&rio,fd);
	rio_readlineb(&rio,buf,MAXLINE);
	sscanf(buf,"%s%s%s",method,uri,version);
	if(strcasecmp(method,"GET")){
		error_request(fd,method,"501","NOT Implemented","weblet does not implement this method");
	}
	read_requesthdrs(&rio);

	static_flag = is_static(uri);
	if(static_flag)
		parse_static_uri(uri,filename);
	else
		parse_dynamic_uri(uri,filename,cgiargs);
	
	if(stat(filename,&sbuf)<0){
		error_request(fd,filename,"404","Not found","weblet could not find this file");
	}

	if(static_flag){
		if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR & sbuf.st_mode)){
			error_request(fd,filename,"403","Forbidden","weblet is not permtted to read the file");
		}
		feed_static(fd,filename,sbuf.st_size);
	}else{
		if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR & sbuf.st_mode)){
			error_request(fd,filename,"403","Forbidden","weblet could not run the CGI program");
		}
		feed_dynamic(fd,filename,cgiargs);
	}
	close(fd);
}

int is_static(char *uri)
{
	if(!strstr(uri,"cgi-bin"))
		return 1;
	else
		return 0;
}
