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
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	
	int s = getaddrinfo("canada", "32400", &hints, &result);
	//int s = getaddrinfo(server, portChar, &hints, &result);
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

	char buf[255] = {0};
	ssize_t nread;

	int sfd;
	
		for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		addr_fam = rp->ai_family;
		addr_len = rp->ai_addrlen;
		if (addr_fam == AF_INET) {
			remote_addr_in = *(struct sockaddr_in *)rp->ai_addr;
			inet_ntop(addr_fam, &remote_addr_in.sin_addr,
					remote_addr_str, addr_len);
			//remote_port = ntohs(remote_addr_in.sin_port);
			//remote_addr = (struct sockaddr *)&remote_addr_in;
			//local_addr = (struct sockaddr *)&local_addr_in;
		} else {
			remote_addr_in6 = *(struct sockaddr_in6 *)rp->ai_addr;
			inet_ntop(addr_fam, &remote_addr_in6.sin6_addr,
					remote_addr_str, addr_len);
			//remote_port = ntohs(remote_addr_in6.sin6_port);
			//remote_addr = (struct sockaddr *)&remote_addr_in6;
			//local_addr = (struct sockaddr *)&local_addr_in6;
		}

	if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;  /* Success */

		close(sfd);
	}

	if (rp == NULL) {   /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	s = getsockname(sfd, local_addr, &addr_len);
	if (addr_fam == AF_INET) {
		inet_ntop(addr_fam, &local_addr_in.sin_addr,
				local_addr_str, addr_len);
		local_port = ntohs(local_addr_in.sin_port);
	} else {
		inet_ntop(addr_fam, &local_addr_in6.sin6_addr,
				local_addr_str, addr_len);
		local_port = ntohs(local_addr_in6.sin6_port);
	}
	/*fprintf(stderr, "Local socket info: %s:%d (family: %d, len: %d)\n",
			local_addr_str, local_port, addr_fam,
			addr_len);*/

//Don't touch above here for now
	unsigned char output[1024] = {0};
	int location = 0;
	unsigned char nonce[4] = {0};
	int tempNonce = 0;
unsigned char updatePort[4] = {0};


		if (write(sfd, array, 8) != 8) {
			fprintf(stderr, "partial/failed write\n");
			exit(EXIT_FAILURE);
		}

		nread = read(sfd, buf, 127);
		if (nread == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		//printf("Received %zd bytes: %s\n", nread, buf);
		memcpy(&output[location], &buf[1], buf[0]);
		location = location + buf[0];
		if ((buf[buf[0]+1]) != 0){
			
			if((buf[buf[0] + 1]) == 1){
				memcpy(&updatePort, &buf[buf[0] + 2], 2);
				print_bytes(buf, 100);
				ntohs(updatePort);
				print_bytes(updatePort, 8);
				remote_addr_in.sin_port = htons(updatePort);
				remote_addr_in6.sin6_port = htons(updatePort);
			}
		}
		memcpy(&tempNonce, &buf[buf[0]+4], 4);
		tempNonce = ntohl(tempNonce);
		//printf("%x\n", tempNonce);
		
		tempNonce = tempNonce + 1;
		tempNonce = ntohl(tempNonce);
		
		memcpy(&nonce, &tempNonce, 4);
		//printf("Output: %s\n", output);
		//print_bytes(buf, 255);
		//print_bytes(nonce, 4);
		
	struct sockaddr_in to;
	
	while(buf[0] != NULL){
		memset(buf, 0, 255);
		
		/*if (write(sfd, nonce, 4) != 4) {
			fprintf(stderr, "partial/failed write\n");
			exit(EXIT_FAILURE);
		}

		nread = read(sfd, buf, 127);
		if (nread == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}*/
		//printf("port number %d\n", ntohs(ai_addr);
		int st = sendto(sfd, nonce, 4, 0,rp->ai_addr, rp->ai_addrlen);
		nread = recvfrom(sfd, buf, 127, 0, local_addr, sizeof(local_addr));
		print_bytes(buf,256);
		//printf("Received %zd bytes: %s\n", nread, buf);
		//print_bytes(buf, 255);
		memcpy(&output[location], &buf[1], buf[0]);

		location = location + buf[0];
		if ((buf[buf[0]+1]) != 0){
			
			if((buf[buf[0] + 1]) == 0){

				memcpy(&portChar, &buf[buf[0] + 2], 4);
				print_bytes(buf,256);
				//htons(portChar);
				print_bytes(portChar,4);
				remote_addr_in.sin_port = htons(portChar);
			}
		}
		memcpy(&tempNonce, &buf[buf[0]+4], 4);
		tempNonce = ntohl(tempNonce);
		//printf("%x\n", tempNonce);
		
		tempNonce = tempNonce + 1;
		tempNonce = ntohl(tempNonce);
		
		memcpy(&nonce, &tempNonce, 4);
		
	
	//printf("Char %d" , buf[0]);
	}
//print_bytes(output,1024);
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
