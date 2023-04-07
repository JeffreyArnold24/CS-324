#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd();
void handle_client();
void *thread(void *descriptor);
static int byte_cnt;  /* Byte counter */
static sem_t mutex;   /* and the mutex that protects it */
static void init_echo_cnt(void);
int slow;

typedef struct {
   int *buf; //Buffer array
   int n; //Maximum number of slots
   int front; //buf[front+1%n] is first item
   int rear; //buf[rear%n] is last item
   sem_t mutex; //Protects accesses to buf
   sem_t slots; //Counts abailable slots
   sem_t items; //Counts abailable items
} sbuf_t;

typedef struct {
	int rio_fd; /* descriptor for this internal buf */
	int rio_cnt; /* unread bytes in internal buf */
	char *rio_bufptr; /* next unread byte in internal buf */
	char rio_buf[8192]; /* internal buffer */
} rio_t;

void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);


void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);



sbuf_t sbuf;

int main(int argc, char *argv[])
{
	//test_parser();
	pthread_t tid[8];
	int sfd = open_sfd(argv[1]);

	struct sockaddr_storage addr;
 	socklen_t addr_len = sizeof(addr);
	sbuf_init(&sbuf, 5);
	for (int i = 0; i < 8; i++){
		pthread_create(&tid[i], NULL, thread, NULL); 
	}
	while(1){
		addr_len = sizeof(addr);
		int sfd2 = accept(sfd, (struct sockaddr *)&addr, &addr_len);
		if (sfd2 < 0){
			printf("Could not accept: %s\n", strerror(errno));
		}
		//pthread_create(&tid, NULL, thread, sfd2);
		sbuf_insert(&sbuf, sfd2);
		//close(sfd2);
	}
	printf("%s\n", user_agent_hdr);
	return 0;
}

void *thread(void *descriptor){
	//int id = *((int *)descriptor);
	pthread_detach(pthread_self());
	while (1) {
		int connfd = sbuf_remove(&sbuf);
		handle_client(connfd);
		close(connfd);
	}
	//close(id);
	//return NULL;
}

void handle_client(int sfd){
	rio_t rio;
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, init_echo_cnt);
	char temp[MAX_OBJECT_SIZE] = {0};
	char buf[MAX_OBJECT_SIZE] = {0};
	char res[255] = {0};
	char method[16], hostname[64], port[8], path[64], headers[1024] = {0};
	rio_readinitb(&rio, sfd); 
	//rio_readlineb(&rio, buf, 255);
	
	//int r = read(sfd,buf, 255,0);
	int loc2 = 0;
	int r = 0;
printf("Here\n");
	while (1){
		r = recv(sfd,temp, 255,0);
		memcpy(&buf[loc2], &temp, 255);
		loc2 = loc2 + r;
		printf("Amount read: %d %d\n",r, sfd);
		printf("Temp: %s\n", temp); 
		printf("Buf: %s\n", buf); 
		if (all_headers_received(buf)){
			break;
		}
	}
	//printf("Amount read: %d\n",r);
	//sem_wait(&mutex);
	
	if (parse_request(buf, method, hostname, port, path, headers)) {
			//printf("METHOD: %s\n", method);
			//printf("HOSTNAME: %s\n", hostname);
			//printf("PORT: %s\n", port);
			//printf("HEADERS: %s\n", headers);
			//printf("Path: %s\n", path);
	} else {
		printf("REQUEST INCOMPLETE\n");
	}
	
	
	char request[MAX_OBJECT_SIZE] = {0};
	strcat(request, method);
	strcat(request, " ");
	strcat(request, path);
	strcat(request, " ");
	strcat(request, "HTTP/1.0\r\n");
	strcat(request, "Host: ");
	strcat(request, hostname);
	if (strcmp("80", port) != 0){
		strcat(request, ":");
		strcat(request, port);
	}
	
	strcat(request, "\r\n");
	strcat(request, "User-Agent: ");
	strcat(request, user_agent_hdr);
	strcat(request, "\r\n");
	strcat(request, "Connection: close\r\n");
	strcat(request, "Proxy-Connection: close\r\n\r\n");
	//printf("Request: %s\n", request);
	
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	hints.ai_family = AF_INET;	/* Choose IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	/* For wildcard IP address */
	hints.ai_protocol = 0;		  /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	int sfd2;
	int s = getaddrinfo(hostname, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	
	
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd2 = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd2 < 0) {
			printf("Could not create socket: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (connect(sfd2, rp->ai_addr, rp->ai_addrlen) != -1) {
			continue;
		}
	}
	/*if (rp == NULL){
		printf("Could not connect: %s\n", strerror(errno));
			exit(EXIT_FAILURE);	
	}*/
	
	int i = 0;
	
	i = send(sfd2, request, 255, 0);

	if (i < 0){
		printf("Could not send: %s\n", strerror(errno));
	}
	//print_bytes(request,255);
	int loc = 0;
	while (i != 0){
	//while ((i != 0) && all_headers_received(res)){
	//while (!all_headers_received(res)){
		i = read(sfd2, res, 255);
		if (i < 0){
			printf("Could not read (a): %s %d\n", strerror(errno), sfd2);
			exit(0);
		}
		memcpy(&buf[loc], &res, 255);
		loc = loc + i;
		//printf("%d\n" , i);
		/*if(all_headers_received(res)){
			break;
		}*/
	}
	//printf("Response: %s\n", res);
	close(sfd2);
	int w = rio_writen(sfd, buf, loc);
	//int w = write(sfd,buf, loc,0);
	if (w < 0){
		printf("Could not write: %s\n", strerror(errno));
	}
	//sem_post(&mutex);
	//printf("Finish: %d\n", sfd);
	close(sfd);
	
	freeaddrinfo(result);
}

int open_sfd(char *portNumber){

	struct addrinfo hints;
	struct addrinfo *result;
	hints.ai_family = AF_INET;	/* Choose IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	/* For wildcard IP address */
	hints.ai_protocol = 0;		  /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	int s = getaddrinfo(NULL, portNumber, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	//short portB = atoi(portNumber);
	int sfd = socket(result->ai_family, result->ai_socktype, 0);
	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (bind(sfd, result->ai_addr, result->ai_addrlen) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sfd,100) < 0){
		printf("Could not Listen\n");
		printf("%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int all_headers_received(char *request) {
	if (strstr(request, "\r\n\r\n") != NULL){
		return 1;
	}
	return 0;
}

int parse_request(char *request, char *method,
		char *hostname, char *port, char *path, char *headers) {
	int strLen = strlen(request);
	char url[MAX_OBJECT_SIZE] = {0};
	int location = 0;
	char str[MAX_OBJECT_SIZE] = {0};
	strcpy(str, request);
	int urlLen = 0;
	for (int i = 0; i < strLen; ++i){
		if (str[i] == ' '){
			str[i] = '\0';
			method = strncpy(method, str, i+1);
			location = i + 1;
			break;
		}
	}
	
	for (int j = location; j < strLen; ++j){
		if (str[j] == ' '){
			str[j] = '\0';
			memcpy(&url, &str[location], j+1-location);
			urlLen = j;
			urlLen = urlLen-location;
			location = j + 1;		
			break;
		}
	}
	for (int j = location; j < strLen; ++j){
		if ((str[j] == '\r') && (str[j+1] == '\n')){
			str[j] = '\0';
			memcpy(headers, &str[location], j+1-location);		
			break;
		}
	}
	bool colon = false;
	for (int k = 0; k < urlLen; ++k){
		if (url[k] == ':'){
			k = k + 3;
			int p = k;
			while((url[k] != ':' ) && (url[k] != '/' )){
				k = k + 1;
			}
			
			if (url[k] == ':'){
				colon = true;
			}			
			url[k] = '\0';
			
			memcpy(hostname, &url[p], k-6);
			p = k;
			if (colon){
				while(url[k] != '/' ){
					k = k+1;
				}
				url[k] = '\0';
				memcpy(port, &url[p+1], k-p);
				url[k] = '/';
				url[urlLen] = '\0';
				memcpy(path, &url[k], urlLen-k+1);
			}
			else {
				char *defaultPort = "80";
				memcpy(port, defaultPort, 2);
				url[k] = '/';
				url[urlLen] = '\0';
				memcpy(path, &url[k], urlLen-k+1);
				
			}		
			break;					
			
		}
	}	
	return 1;
}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64], headers[1024];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path, headers)) {
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("HEADERS: %s\n", headers);
			printf("Path: %s\n", path);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
}

void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}

void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */  
}

void sbuf_deinit(sbuf_t *sp) {
   free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item) {
    sem_wait(&sp->slots);                       
    sem_wait(&sp->mutex);                         
    sp->buf[(++sp->rear)%(sp->n)] = item;  
    sem_post(&sp->mutex);                          
    sem_post(&sp->items);                         
}

int sbuf_remove(sbuf_t *sp) {
    int item;
    sem_wait(&sp->items);                         
    sem_wait(&sp->mutex);                       
    item = sp->buf[(++sp->front)%(sp->n)]; 
    sem_post(&sp->mutex);                      
    sem_post(&sp->slots);                  
    return item;
}

static void init_echo_cnt(void)
{
    sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;
    slow = 0;
    for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* EOF, no data read */
	    else
		break;    /* EOF, some data was read */
	} else
	    return -1;	  /* Error */
    }
    *bufp = 0;
    return n-1;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	printf("Here: Slow?");
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF */
	    return 0;
	else 
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
