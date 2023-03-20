// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1000

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	
	
	unsigned char array[8];
	char *server = argv[1];
	int portInt = atoi(argv[2]);
	char *portChar = argv[2];
	int level = atoi(argv[3]);
	int seed = atoi(argv[4]);
	//int level = argv[3];
	//int seed = argv[4];
	
	unsigned int hex_user;
	unsigned short hex_seed;
	
	
	bzero(array,8);
	array[0] = 0;
	int hostindex;
	
	array[1] = level;
	hex_user = USERID;
	hex_user = ntohl(hex_user);
	

	memcpy(&array[2], &hex_user, 4);

	hex_seed = htons(seed);	
	memcpy(&array[6], &hex_seed, 2);

	//print_bytes(array, 8);

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	//printf("%s, %s\n", server, portChar);	
	int s = getaddrinfo(server, portChar, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE); 
	}
	
//Up to Here is good

	char remote_addr_str[INET6_ADDRSTRLEN];
	int addr_fam;
	socklen_t addr_len;

	struct sockaddr_in remote_addr_in;
	struct sockaddr_in6 remote_addr_in6;
	struct sockaddr *remote_addr;

	struct sockaddr_in local_addr_in;
	struct sockaddr_in6 local_addr_in6;
	struct sockaddr *local_addr;
	char local_addr_str[INET6_ADDRSTRLEN];
	unsigned short local_port;

	char buf[256] = {0};
	ssize_t nread;

	int sfd;
		sfd = socket(result->ai_family, result->ai_socktype,
				result->ai_protocol);

		addr_fam = result->ai_family;
		addr_len = result->ai_addrlen;
		if (addr_fam == AF_INET) {
			remote_addr_in = *(struct sockaddr_in *)result->ai_addr;
			inet_ntop(addr_fam, &remote_addr_in.sin_addr,
					remote_addr_str, addr_len);
			//remote_port = ntohs(remote_addr_in.sin_port);
			//remote_addr = (struct sockaddr *)&remote_addr_in;
			local_addr = (struct sockaddr *)&local_addr_in;
		} 

		int g = bind(sfd, local_addr, result->ai_addrlen);
		if (g == -1){
			
			printf("Could not Connect\n");
			exit(EXIT_FAILURE);
		}
struct sockaddr_in sin = {};
s = getsockname(sfd, (struct sockaddr *)&sin, sizeof(sin));
fprintf(stderr, "Local socket info: %d\n", sin.sin_addr);
	


	freeaddrinfo(result);

	s = getsockname(sfd, local_addr, &addr_len);
	if (addr_fam == AF_INET) {
		inet_ntop(addr_fam, &local_addr_in.sin_addr,
				local_addr_str, addr_len);
		local_port = ntohs(local_addr_in.sin_port);
	}
	

//Don't touch above here for now
	unsigned char output[1024] = {0};
	int location = 0;
	unsigned char nonce[4] = {0};
	int tempNonce = 0;
	unsigned char updatePort[4] = {0};
	unsigned short newPort;
	int op;

		int st = sendto(sfd, array, 8, 0,(struct sockaddr *)&remote_addr_in, sizeof(remote_addr_in));
		nread = recvfrom(sfd, buf, 127, 0, local_addr, sizeof(local_addr));

		memcpy(&output[location], &buf[1], buf[0]);
		location = location + buf[0];
		op = buf[buf[0]+1];
		if (op != 0){
			
			if(op == 1){
				memcpy(&newPort, &buf[buf[0] + 2], 2);
				remote_addr_in.sin_port = newPort;
			}

			if(op == 2){

				memcpy(&newPort, &buf[buf[0] + 2], 2);
				close(sfd);
				sfd = socket(AF_INET, SOCK_DGRAM,
				0);
				memset(&local_addr_in, 0, sizeof(local_addr_in));
				local_addr_in.sin_family = AF_INET;
				local_addr_in.sin_port = htons(newPort);
				local_addr_in.sin_addr.s_addr = 0;
				
				local_addr = (struct sockaddr *)&local_addr_in;
				g = bind(sfd, local_addr, sizeof(local_addr_in));
				if (g == -1){
					printf("Could not Connect\n");
					exit(EXIT_FAILURE);
				}
				

			}

			if(op == 3){
				struct sockaddr_in tempSock;
				memcpy(&newPort, &buf[buf[0] + 2], 2);
				printf("%d\n", newPort);
				for (int i = 0; i < newPort; ++i){
					
					recvfrom(sfd, buf, 256, 0, (struct sockaddr *)&tempSock, sizeof(tempSock));
					printf("%d\n", tempSock.sin_port);

				}
			}
		}
		memcpy(&tempNonce, &buf[buf[0]+4], 4);
		tempNonce = ntohl(tempNonce);
		
		tempNonce = tempNonce + 1;
		tempNonce = ntohl(tempNonce);
		
		memcpy(&nonce, &tempNonce, 4);
print_bytes(buf,256);

	while(buf[0] != NULL){

		memset(buf, 0, 255);

		int st = sendto(sfd, nonce, 4, 0,(struct sockaddr *)&remote_addr_in, sizeof(remote_addr_in));

		nread = recvfrom(sfd, buf, 256, 0, local_addr, sizeof(local_addr));
print_bytes(buf,256);
		memcpy(&output[location], &buf[1], buf[0]);

		location = location + buf[0];
		op = buf[buf[0]+1];

		if (op != 0){
			
			if(op == 1){
				memcpy(&newPort, &buf[buf[0] + 2], 2);
				remote_addr_in.sin_port = newPort;				
			}
			if(op == 2){

				memcpy(&newPort, &buf[buf[0] + 2], 2);
				close(sfd);
				sfd = socket(AF_INET, SOCK_DGRAM,
				0);
				memset(&local_addr_in, 0, sizeof(local_addr_in));
				local_addr_in.sin_family = AF_INET;
				local_addr_in.sin_port = htons(newPort);
				local_addr_in.sin_addr.s_addr = 0;
				
				local_addr = (struct sockaddr *)&local_addr_in;
				g = bind(sfd, local_addr, sizeof(local_addr_in));
				if (g == -1){
					printf("Could not Connect\n");
					exit(EXIT_FAILURE);
				}

			}
			if(op == 3){
				struct sockaddr_in tempSock;
				socklen_t foo = sizeof(tempSock);
				memcpy(&newPort, &buf[buf[0] + 2], 2);
				printf("Number %d\n", ntohs(newPort));
				for (int i = 0; i < ntohs(newPort); ++i){
					
					recvfrom(sfd, buf, 256, 0, (struct sockaddr *)&tempSock, &foo);
					printf("%d\n", ntohs(tempSock.sin_port));

				}
			}
		}

		memcpy(&tempNonce, &buf[buf[0]+4], 4);
		tempNonce = ntohl(tempNonce);		
		tempNonce = tempNonce + 1;
		tempNonce = ntohl(tempNonce);
		memcpy(&nonce, &tempNonce, 4);


	}
fprintf(stdout,"%s\n", output);
exit(EXIT_SUCCESS);
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
