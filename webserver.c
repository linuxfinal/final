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
	pthread_t tid;//create thread id
	struct sockaddr_in clientaddr;
	if(argc!=2){
		fprintf(stderr,"usage: %s<port>\n",argv[0]);
		exit(1);
	}
	port=atoi(argv[1]);

	listen_sock = open_listen_sock(port);
	clientlen = sizeof(clientaddr);
	while(1){
		
		conn_sock = malloc(sizeof(int));
		*conn_sock = accept(listen_sock,(SA *)&clientaddr,&clientlen);
		pthread_create(&tid,NULL,process_trans,conn_sock);
	}
}

void *process_trans(void *vargp)//()
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
		//return;
	}
	
	read_requesthdrs(&rio);
	static_flag = is_static(uri);
	if(static_flag)
		parse_static_uri(uri,filename);
	else
		parse_dynamic_uri(uri,filename,cgiargs);
	
	if(stat(filename,&sbuf)<0){
		error_request(fd,filename,"404","Not found","weblet could not find this file");
		//return;
	}

	if(static_flag){
		if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR & sbuf.st_mode)){
			error_request(fd,filename,"403","Forbidden","weblet is not permtted to read the file");
			//return;
		}
		feed_static(fd,filename,sbuf.st_size);
	}else{
		if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR & sbuf.st_mode)){
			error_request(fd,filename,"403","Forbidden","weblet could not run the CGI program");
			//return;
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

void error_request(int fd,char *cause,char *errnum,char *shortmsg, char *description)
{
	char buf[MAXLINE],body[MAXBUF];
	sprintf(body,"<html><title>error request</title>");
	sprintf(body,"%s<body bgcolor=""ffffff"">\r\n",body);
	sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
	sprintf(body,"%s<p>%s:%s\r\n",body,description,cause);
	sprintf(body,"%s<hr><em>weblet Web server</em>\r\n",body);

	sprintf(buf,"HTTP/1.0 %s%s\r\n",errnum,shortmsg);
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-type:text/html\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-length:%d\r\n\r\n",(int)strlen(body));
	rio_writen(fd,buf,strlen(buf));
	rio_writen(fd,body,strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
	char buf[MAXLINE];
	rio_readlineb(rp,buf,MAXLINE);
	while(strcmp(buf,"\r\n")){
		printf("%s",buf);
		rio_readlineb(rp,buf,MAXLINE);
	}
	return;
}

void parse_static_uri(char *uri,char *filename)
{
	char *ptr;
	strcpy(filename,".");
	strcat(filename,uri);
	if(uri[strlen(uri)-1]=='/')
		strcat(filename,"test.html");
}

void parse_dynamic_uri(char *uri,char *filename,char *cgiargs)
{
	char *ptr;
	ptr = index(uri,'?');
	if(ptr){
		strcpy(cgiargs,ptr+1);
		*ptr='\0';
	}else
		strcpy(cgiargs,"");
	strcpy(filename,".");
	strcat(filename,uri);
}

void feed_static(int fd,char *filename,int filesize)
{
	int srcfd;
	char *srcp,filetype[MAXLINE],buf[MAXBUF];
	get_filetype(filename,filetype);
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	sprintf(buf,"%sServer:weblet Web Server\r\n",buf);
	sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
	sprintf(buf,"%sContent-type:%s\r\n\r\n",buf,filetype);
	rio_writen(fd,buf,strlen(buf));
	srcfd=open(filename,O_RDONLY,0);
	srcp=mmap(0,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);
	close(srcfd);	
	rio_writen(fd,srcp,filesize);
	munmap(srcp,filesize);
}

void feed_dynamic(int fd,char *filename,char *cgiargs)
{
	char buf[MAXLINE],*emptylist[]={NULL};	
	int pfd[2];
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Server:weblet Web Server\r\n");
	rio_writen(fd,buf,strlen(buf));

	pipe(pfd);
	if(fork()==0){
		close(pfd[1]);
		dup2(pfd[0],STDIN_FILENO);
		dup2(fd,STDOUT_FILENO);
		execve(filename,emptylist,environ);
	}
	close(pfd[0]);
	write(pfd[1],cgiargs,strlen(cgiargs)+1);
	wait(NULL);
	close(pfd[1]);
}
void get_filetype(char *filename,char *filetype)
{
	if(strstr(filename,".html")) strcpy(filetype,"text/html");
	else if (strstr(filename,".jpg")) strcpy(filetype,"image/jpeg");
	else if (strstr(filename,".mpeg")) strcpy(filetype,"video/mpeg");
	else strcpy(filetype,"text/html");
}
