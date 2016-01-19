#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#define MAXLINE 1024
#define IPADDRESS "127.0.0.1"
#define SERV_PORT 8786

#define max(a,b) (a > b) ? a : b

static int handle_recv_msg(int sockfd, struct sockaddr_in addr, char *buf) 
{
	int n;
    assert(buf);
    printf("client recv msg is:%s\n", buf);
	fgets(buf, MAXLINE, stdin);   
    n = sendto(sockfd, buf, strlen(buf) +1, 0, (struct sockaddr * ) & addr, sizeof(addr));
	if (n < 0)  
	{  
		perror("sendto error");  
		return -1;  
	}  
    return 0;
}

static void recv_udp_serv_msg(int sockfd, struct sockaddr_in addr)
{
	char sendline[MAXLINE],recvline[MAXLINE];
	int maxfdp,stdineof;
	fd_set readfds;
	int n;
	socklen_t len = sizeof(addr);
	struct timeval tv;
	int retval = 0;

	while (1) 
	{
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		maxfdp = sockfd;

		tv.tv_sec = 5;
		tv.tv_usec = 0;

		retval = select(maxfdp+1, &readfds, NULL, NULL, &tv);

		if (retval == -1) {
			return ;
		}

		if (retval == 0) {
			printf("client timeout.\n");
			continue;
		}

		if (FD_ISSET(sockfd, &readfds)) {
			n = recvfrom(sockfd, recvline, MAXLINE, 0, (struct sockaddr *) &addr, &len);  
			if (n <= 0) {
				fprintf(stderr,"client: server is closed.\n");
				close(sockfd);
				FD_CLR(sockfd, &readfds);
				return;
			}

			handle_recv_msg(sockfd, addr, recvline);
			}
		}
	}

int main(int argc,char *argv[])
{
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
	inet_pton(AF_INET, IPADDRESS, &servaddr.sin_addr);

	printf("client send to server .\n");
	sendto(sockfd, "hello server", 32, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

	recv_udp_serv_msg(sockfd, servaddr);

	return 0;												
}