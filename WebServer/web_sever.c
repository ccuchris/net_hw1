#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/sendfile.h>

char web_page[8192]="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<img src=\"pinapple.jpeg\" alt=\"Revealing\" title=\"Demo picture\" width=\"200px\">\r\n"
"<form method=\"post\" enctype=\"multipart/form-data\">\r\n"
"<p><input type=\"file\" name=\"upload\"></p>\r\n"
"<p><input type=\"submit\" value=\"submit\"></p>\r\n"
"</center>\r\n"
"</body></html>\r\n";

static void sigchld_handler(int sig)
{
	pid_t pid;
	while((pid=waitpid(-1, NULL, WNOHANG))>0);
}

void process(int clientSocket)
{
	char buf[65536];
	char filename[1024];
	char *temp;
	char *temp1;
	int img_fd, len, count=0;
	FILE *fptr;	//在伺服器上建的檔案

	memset(buf, 0, sizeof(buf));
	read(clientSocket, buf, sizeof(buf));	//讀取瀏覽器要求

	if(strncmp(buf, "GET /pinapple.jpeg", 17)==0){                       // picture
		img_fd = open("pinapple.jpeg", O_RDONLY);	//取得fd
		sendfile(clientSocket, img_fd, NULL, 5000000);
		close(img_fd);	//將fd從檔案表中移出
	}
	else if(strncmp(buf, "POST /", 6)==0){                              // file
		temp = strstr(buf, "filename");		//會在buf中搜尋filename這個字串
		if(temp!=NULL){
			len = strlen("filename");
			temp = temp + len + 2;	//+2是因為="
			while(*temp!='\"'){
				filename[count++]=*temp;	//讀到檔案名稱尾巴
				temp++;
			}
			++temp;
			filename[count]='\0';
			while(!(*(temp-4)=='\r' && *(temp-3)=='\n' && *(temp-2)=='\r' && *(temp-1)=='\n')) ++temp;   //file begin
			printf("receive filename: %s\n", filename);
            if((fptr=fopen(filename,"w"))==NULL){
                printf("error:file");
                exit(1);
            }

			temp1=buf+strlen(buf)-3;
			while(*temp1!='\r') --temp1;	//文件結尾
			--temp1;
			while(temp!=temp1){
				fputc(*temp,fptr);
				temp++;
			}
			fputc(*temp,fptr);
			fclose(fptr);
			write(clientSocket, web_page, sizeof(web_page));
		}
		else 	//處理GET / 的要求，將網頁內容輸出到客戶端
			write(clientSocket, web_page, sizeof(web_page));		
	}
	else //if(strncmp(buf, "GET /", 5)==0)                               // html
		write(clientSocket, web_page, sizeof(web_page));
}

void print_info(int fdsock, int which)
{
	socklen_t addresslen;
	struct sockaddr_in address;
	switch(which){
		case 1:
			printf("\n\tServer listen address: ");
			break;
		case 2:
			printf("Connected server address: ");
			break;	
	}
	getsockname(fdsock, (struct sockaddr *)&address, &addresslen);
	printf("%s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
}

int main(void)
{
	int serverSocket;
	struct sockaddr_in serverAddress;
	int clientSocket;
	//FILE *fptr_html = fopen("index.html", "r");
	//setHttpHeader(fptr_html);
	//fclose(fptr_html);

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);                                // create the socket

	bzero(&serverAddress, sizeof(serverAddress));                                  // initialize structure server
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));	//網路監視器

	int listening = listen(serverSocket, 10);	//listen
                                          
	signal(SIGCHLD, sigchld_handler);	// prevent zombie process

	print_info(listening, 1);

	while(1) {
		clientSocket = accept(serverSocket, NULL, NULL);
		pid_t child = fork();	//fork出子行程
		if(child==0){
			close(listening);
			print_info(clientSocket, 2);
			process(clientSocket);
			close(clientSocket);
			//printf("connected socket is closing....\n");
			exit(0);	
		}
		close(clientSocket);
	}
	return 0;
}
