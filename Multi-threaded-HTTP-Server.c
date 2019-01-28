#include<stdio.h>
#include<string.h>    // for strlen
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> // for inet_addr
#include<unistd.h>    // for write
#include<pthread.h>   // for threading, link with lpthread
#include <semaphore.h>// for locking area for limit
#include <sys/types.h>// for file operations
#include <sys/stat.h>
#include <fcntl.h>

//##localhost kullanıldı(INADDR_ANY)
#define port_no 2345
char *html_path="/home/cme2002/Desktop/opSys/index.html";
char *jpeg_path="/home/cme2002/Desktop/opSys/icon.jpeg";
//##!!!Dosya isimleri icon.jpeg  ve index.html olmalı.(Değilse Hata Verecektir)

//kapasite için semaphore tanımlandı.
sem_t capacity;

unsigned long fileSize(char* file)
{   //Client'a gönderilecek verinin size'ı döndürüldü.
    FILE * f = fopen(file, "r");
    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long)ftell(f);
    fclose(f);
    return len;
}
 
void *connection_handler(void *socket_desc)
{
	
	//İsteğin alınması için alan oluşturuldu.& İstenilen veri yolu belirlendi.
	char buf[2048];
	int fd;//file descriptor
	char *message;
	
	//Get the socket descriptor
    int sock = *((int*)socket_desc);
	
    //Değişkenlere soket verileri atandı.
	memset(buf, 0, 2048);
	read(sock, buf, 2047);
	
	//Gelen isteğe göre yönlendirme yapıldı.
	if(!strncmp(buf, "GET /icon.jpeg", 14)) {//jpeg dosyası için yapı kuruldu
		fd = open(jpeg_path, O_RDONLY);
		if(fd==-1){//dosya farklı konumdaysa hata kontrolü yapıldı.
			puts("HTTP/1.1 301 Moved Permanently\n\n");
			message="ERROR\nRequested object moved, new location specified later in this message!\n";
			write(*((int*)socket_desc), message, strlen(message));
			close(fd);
			close(*((int*)socket_desc));
			return 0;
		}
		else{
		sendfile(sock, fd, NULL, fileSize(jpeg_path));
		puts("HTTP/1.1 200 OK\nConnection: close\nContent-Type: image/jpeg\n\n");
		close(fd);
		}
	} else if(!strncmp(buf, "GET /index.html", 15))	{//html dosyası için yapı kuruldu
		fd = open(html_path, O_RDONLY);
		if(fd==-1){//dosya farklı konumdaysa hata kontrolü yapıldı.
			puts("HTTP/1.1 301 Moved Permanently\n\n");
			message="ERROR\nRequested object moved, new location specified later in this message!\n";
			write(*((int*)socket_desc), message, strlen(message));
			close(fd);
			close(*((int*)socket_desc));
			return 0;
		}
		else{
		sendfile(sock, fd, NULL, fileSize(html_path));
		puts("HTTP/1.1 200 OK\nConnection: close\nContent-Type: text/html\n\n");
		close(fd);
		}
	}else 	{
		puts("HTTP/1.1 400 Bad Request\n\n");
		message="ERROR\nRequest message not understood by server!\n";
		write(*((int*)socket_desc), message, strlen(message));
		//kontrolümüz dışında gelen isteklerin 10luk sınırı bozmaması sağlandı.
		sem_post(&capacity);
	}
	
    close(*((int*)socket_desc));	//Close the socket pointer
	
    return 0;
}
int main(int argc, char *argv[])
{
	//10 isteğe cevap verebilmek için tanımlama yapıldı.
	sem_init(&capacity, 0, 10);
	
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;
	
     
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        puts("Could not create socket");
        return 1;
    }
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_no);
     
    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("Binding failed");
        return 1;
    }
     
    listen(socket_desc,3);
     
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
	int value;
	char *message;
    while(new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))
    {
		sem_getvalue(&capacity, &value);
		if(value==0){//10 istekten sonrasına hizmet verilmemesi sağlandı.
			message = "Server Is Busy!\n";
			write(new_socket, message, strlen(message));
			close(new_socket);
		}else{
			sem_wait(&capacity);
		
			if(new_socket==1)
			{
				puts("Connection Failed.");
				return 1;
			}
		
			puts("Connection accepted");
         
			pthread_t sniffer_thread;
			new_sock = malloc(1); // a memory allocator
			*new_sock = new_socket;
			
			if(pthread_create(&sniffer_thread, NULL, connection_handler,
							(void*)new_sock) < 0)
			{
				puts("Could not create thread");
				return 1;
			}

			puts("Handler assigned");
		}
    }     
    return 0;
}