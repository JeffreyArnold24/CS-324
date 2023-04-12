#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"					// ADDED TO PROJ (also added sbuf.c)
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// From echoservert_pre.c
#define MAXLINE 8192
#define NTHREADS  100
#define SBUFSIZE  20

// For proxy.c - /* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// From http_parser
#define HTTP_REQUEST_MAX_SIZE 4096
#define HOSTNAME_MAX_SIZE 512
#define PORT_MAX_SIZE 6
#define URI_MAX_SIZE 4096
#define METHOD_SIZE 32

// From hw5 client.c
#define BUF_SIZE 20000

// For echoservert_pre.c thread
sbuf_t sbuf; /* Shared buffer of connected descriptors */

// For proxy.c - * You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_conn_hdr = "Proxy-Connection: close\r\n";

// From http_parser
// Returns 1 if complete, 0 if incomplete
int is_complete_request(const char *request) {
	char *found_ptr = strstr(request, "\r\n\r\n");
	if (found_ptr != NULL) {
		return 1;
	}
	return 0;
}

char* get_client_request(int connfd) {
	char* client_req = (char *) malloc(MAX_OBJECT_SIZE);
	int cur_read = 0;	// Reused, num bytes read in a single call 
	char* req_ptr = &client_req[0];

	while ((cur_read = Read(connfd, req_ptr, MAX_OBJECT_SIZE)) > 0) {	// Keeps going while still has bytes being read or until it's complete
		req_ptr += cur_read;

		if (is_complete_request(client_req))
			break;
	}
	strcat(client_req, "\0");	// Denote end of string 

	printf("client_req: %s\n", client_req);
	return client_req;
}

// Based from http_parser
char* reformat_client_request(char *client_req, char *hostname, char *port) {
	// If client has not sent the full request, return 0 to show the request is not complete.
	if (is_complete_request(client_req) == 0) {
		return NULL;
	}

	char* method = (char*) malloc(METHOD_SIZE);
	char* uri = (char*) malloc(URI_MAX_SIZE);

	/*
	 *		Parse request to get the method, hostname, port, uri
	 */
	
	// Make non-const request variable
	char temp_request[500];
	strcpy(temp_request, client_req);

	// Grab method
	char* temp_ptr = strtok(temp_request, " ");
	strcpy(method, temp_ptr);

	// Grab entire path
	temp_ptr = temp_request + strlen(temp_ptr) + 1;  // Move past method
	temp_ptr += 7;	// Move past http://
	temp_ptr = strtok(temp_ptr, " ");

	// Grab everything before slash (Hostname and Port)
	char host_port_str[HOSTNAME_MAX_SIZE + PORT_MAX_SIZE];
	strcpy(host_port_str, temp_ptr);
	strtok(host_port_str, "/");

	// Host and Port
	char* port_str = strstr(host_port_str, ":");
	char host_str[HOSTNAME_MAX_SIZE];
	strcpy(host_str, host_port_str);

	// If port was specified
	if (port_str != NULL) {
		strtok(host_str, ":");
		strcpy(hostname, host_str);
		strcpy(port, port_str + 1);	// Copy port_str into port, skipping the : colon
	} 
	// If just hostname
	else {	// If not colon, make port the default
		strcpy(hostname, host_str);
		strcpy(port, "80");	// Default port number
	}

	// Grab everything after slash (URI), if there is a specific uri
	if (strstr(temp_ptr, "/")) {
		char* uri_str = temp_ptr + strlen(host_port_str) + 1;  // Make uri be everything past the host, port and slash (the +1)
		strcpy(uri, uri_str);
	}

	/*
	 *		Make new request
	 */
	char* server_req = (char*) malloc(HTTP_REQUEST_MAX_SIZE);

	// Concat first line
	strcat(server_req, method);
	strcat(server_req, " /"); // Space and slash for uri
	strcat(server_req, uri);
	strcat(server_req, " HTTP/1.0");	// Version
	strcat(server_req, "\r\n");	// End line

	// Concat second line
	strcat(server_req, "Host: ");
	strcat(server_req, hostname);
	strcat(server_req, ":");
	strcat(server_req, port);
	strcat(server_req, "\r\n");	// End line

	// Concat hard-coded headers
	strcat(server_req, user_agent_hdr);
	strcat(server_req, conn_hdr);
	strcat(server_req, proxy_conn_hdr);

	// Add final \r\n to signify end of file
	strcat(server_req, "\r\n");

	free(method);
	free(uri);

	return server_req;
}

// Based from hw5 client.c
// Sends off request from proxy to the server
int send_request_to_server(char *hostname, char *port, char *server_req) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	// IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_TCP;	// TCP

	// Connect to server

	// Returns a list of address structures, so we try each address until we successfully connect
	Getaddrinfo(hostname, port, &hints, &result);

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */
		close(sfd);
	}
	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);           /* No longer needed */

	// Send request off to server

	char *str_ptr = server_req;
	int chars_left = strlen(str_ptr);
	while (chars_left > 0) {
		int chars_written = Write(sfd, str_ptr, chars_left);
		chars_left -= chars_written;
		str_ptr += chars_written;
	}

	return sfd;
}

int receive_server_response(int sfd, char* server_res) {
	int cur_read = 0;	// Reused, num bytes read in a single call 
	char* res_ptr = &server_res[0];
	int num_bytes_read = 0;

	while ((cur_read = Read(sfd, res_ptr, MAX_OBJECT_SIZE)) > 0) {
		res_ptr += cur_read;
		num_bytes_read += cur_read;
	}
	strcat(server_res, "\0");	// Denote end of string 

	return num_bytes_read;
}

void send_response_to_client(char* server_res, int connfd, int num_bytes_read) {
	char *str_ptr = server_res;
	int chars_left = num_bytes_read;
	while (chars_left > 0) {
		int chars_written = Write(connfd, str_ptr, chars_left);
		chars_left -= chars_written;
		str_ptr += chars_written;
	}
}

void act_as_server_and_client(int connfd) {
	// Part 1 - Client -> Proxy
	char* client_req = get_client_request(connfd);
	char hostname[HOSTNAME_MAX_SIZE];
	char port[PORT_MAX_SIZE];
	char* server_req = reformat_client_request(client_req, hostname, port);	// Reformat in proxy

	// Part 2 - Proxy -> Server
	int sfd = send_request_to_server(hostname, port, server_req);

	// Part 3 - Server -> Proxy
	char* server_res = (char *) malloc(MAX_OBJECT_SIZE);
	int num_bytes_read = receive_server_response(sfd, server_res);

	// Part 4 - Proxy -> Client
	send_response_to_client(server_res, connfd, num_bytes_read);

	free(client_req);
	free(server_req);
}

// Based from echoservert_pre.c
void *thread(void *vargp) 
{  
	pthread_detach(pthread_self()); 
	while (1) { 
		int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
		act_as_server_and_client(connfd);
		close(connfd);
	}
}

// Based off echoservert_pre.c
int main(int argc, char **argv) {
	int i, listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_in ip4addr;
	struct sockaddr_storage clientaddr;
	pthread_t tid; 

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	sbuf_init(&sbuf, SBUFSIZE); //line:conc:pre:initsbuf
	for (i = 0; i < NTHREADS; i++)  /* Create worker threads */ //line:conc:pre:begincreate
		pthread_create(&tid, NULL, thread, NULL);               //line:conc:pre:endcreate

	ip4addr.sin_family = AF_INET;
	ip4addr.sin_port = htons(atoi(argv[1]));
	ip4addr.sin_addr.s_addr = INADDR_ANY;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit(EXIT_FAILURE);
	}
	if (bind(listenfd, (struct sockaddr*)&ip4addr, sizeof(struct sockaddr_in)) < 0) {
		close(listenfd);
		perror("bind error");
		exit(EXIT_FAILURE);
	}
	if (listen(listenfd, 100) < 0) {
		close(listenfd);
		perror("listen error");
		exit(EXIT_FAILURE);
	} 

	while (1) {
		clientlen = sizeof(struct sockaddr_storage);
		connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
	}
    
  return 0;
}