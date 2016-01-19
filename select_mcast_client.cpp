/*============================================================================
File Name:select_mcast_client.cpp
Purples:ѧϰ�ಥԭ��

============================================================================*/
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

#define MAXLINE     1024
#define IPADDR      "127.0.0.2"
#define IPPORT		8888
#define MCASTADDR   "224.0.0.88"
#define MCASTPORT	8785

#define max(a,b) (a > b) ? a : b

#define MCAST_INTERVAL    5 

int main(int argc, char*argv[])
{
	int fd;
	char buf[MAXLINE];
	
	struct sockaddr_in local_addr;
	struct sockaddr_in mcast_addr;

	fd = socket(AF_INET, SOCK_DGRAM, 0); /*�����׽���*/

	if (fd < 0) {
		fprintf(stderr, "socket fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1; 
	}
    
	memset(&mcast_addr, 0, sizeof(mcast_addr));/*��ʼ��IP�ಥ��ַΪ0*/
	mcast_addr.sin_family      = AF_INET; /*����Э��������ΪAF*/
	mcast_addr.sin_addr.s_addr = inet_addr(MCASTADDR);/*���öಥIP��ַ*/
	mcast_addr.sin_port        = htons(MCASTPORT); /*���öಥ Ŀ�Ķ˿ڣ��ಥҲҪָ�������ߵĶ˿ںţ������ܽ����ߵ����ж˿ڶ��������㲥��*/

	/*��ಥ��ַ��������*/

	while(1) 
	{
		int n;
		fgets(buf, MAXLINE, stdin); 
		n = sendto(fd, buf, sizeof(buf), 0, (struct sockaddr*) &mcast_addr, sizeof(mcast_addr));
		
		if( n < 0) {
			fprintf(stderr, "socket fail,error:%d,reason:%s\n", errno, strerror(errno));
			return -2;
		}
	}
	sleep(MCAST_INTERVAL);    /*�ȴ�һ��ʱ��*/

	return 0;
}