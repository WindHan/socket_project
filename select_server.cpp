/*============================================================================
File Name:select_server.cpp
Purples:ѧϰselect����

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

#define IPADDR      "127.0.0.1"
#define MCASTADDR   "224.0.0.88"
#define TCP_PORT    8787
#define UDP_PORT	8786
#define MCASTPORT	8785
#define MAXLINE     1024
#define LISTENQ     5
#define SIZE        10


/*=============================== ȫ�ֶ��� =====================================*/
typedef struct server_context_st
{
    int cli_cnt;        /*�ͻ��˸���*/
    int clifds[SIZE];   /*�ͻ��˸���*/
    fd_set allfds;      /*�������*/
    int maxfd;          /*������ֵ*/
} server_context_st;
/*============================================================================*/

/*=============================== ȫ�ֱ��� =====================================*/
static server_context_st *s_srv_ctx = NULL;
/*============================================================================*/

/*=============================== �������� =====================================*/
static int create_server_tcp_proc(const char* ip,int port);
static int create_server_udp_proc(const char* ip,int port);
static int create_mcast_proc(const char* ip,int port);
static int handle_client_proc(int srvfd);
static int accept_client_proc(int srvfd);
static int recv_tcp_client_msg(fd_set *readfds);
static int recv_udp_client_msg(int srvfd);
static int recv_mcast_msg(int srvfd);
static int handle_tcp_client_msg(int fd, char *buf);
static int handle_udp_client_msg(int fd, struct sockaddr_in addr, char *buf) ;
static int handle_mcast_msg(int srvfd, struct sockaddr_in addr, char *buf);
static void server_uninit();
static int server_init();
/*============================================================================*/

/*=============================== ����ʵ�� =====================================*/  
static int create_server_tcp_proc(const char* ip,int port)
{
    int  fd;
    struct sockaddr_in servaddr;
    fd = socket(AF_INET, SOCK_STREAM,0);
    if (fd  < 0) {
		fprintf(stderr, "create socket fail,error:%d,reason:%s\n", errno, strerror(errno));
        return -1;
    }

	//�����׽���ѡ��������ñ��ص�ַ�Ͷ˿�
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {  
		fprintf(stderr, "setsockopt fail,error:%d,reason:%s\n", errno, strerror(errno));
        return -2;
    }
	
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    if (bind(fd,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0) {
		fprintf(stderr, "bind fail,error:%d,reason:%s\n", errno, strerror(errno));
        return -3;
    }

    listen(fd,LISTENQ);

    return fd;
}

static int create_server_udp_proc(const char* ip,int port)
{
    int  fd;
    struct sockaddr_in servaddr;
    fd = socket(AF_INET, SOCK_DGRAM,0);
    if (fd < 0) {
        fprintf(stderr, "create socket fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -1;
    }
	//�����׽���ѡ��������ñ��ص�ַ�Ͷ˿�
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {  
		fprintf(stderr, "setsockopt REUSEADDR fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -2;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*) & servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "bind fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -3;
    }

    return fd;
}

static int create_mcast_proc(const char* ip,int port)
{
    int  fd;
	struct sockaddr_in mcast_addr;
	struct ip_mreq mreq; 
	
	struct sockaddr_in local_addr;  
	
	fd = socket(AF_INET, SOCK_DGRAM,0);
    if (fd < 0) {
        fprintf(stderr, "create socket fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -1;
    }
    
	/*��ʼ����ַ*/
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port        = htons(port);
   
    if(bind(fd, (struct sockaddr*) &local_addr, sizeof(local_addr)) < 0)
    {
        fprintf(stderr, "socket bind fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -2;
    }

	/*����ಥ��*/
	mreq.imr_multiaddr.s_addr = inet_addr(ip); /*�ಥ��ַ����*/
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);/*����ӿ�ΪĬ��*/
	
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(ip_mreq)) < 0) {
		fprintf(stderr, "setsockopt MEMBERSHIP fail,erron:%d,reason:%s\n", errno, strerror(errno));
		return -3;
	}
	
    return fd;
}

static int handle_client_proc(int srvfd[])
{
    int  clifd = -1;
    int  retval = 0;
    fd_set *readfds = &s_srv_ctx->allfds;
    struct timeval tv;
    int i = 0;

    while (1) {

        /*ÿ�ε���selectǰ��Ҫ���������ļ���������ʱ�䣬��Ϊ�¼��������ļ���������ʱ�䶼���ں��޸���*/
        /*��Ӽ����׽���*/
        FD_ZERO(readfds);
        FD_SET(srvfd[0], readfds);
        s_srv_ctx->maxfd = srvfd[0];
		
		FD_SET(srvfd[1], readfds);
        s_srv_ctx->maxfd = srvfd[1];
		
		FD_SET(srvfd[2], readfds);
        s_srv_ctx->maxfd = srvfd[2];

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        /*��ӿͻ����׽���*/
        for (i = 0; i < s_srv_ctx->cli_cnt; i++) {
            clifd = s_srv_ctx->clifds[i];
            FD_SET(clifd, readfds);
            s_srv_ctx->maxfd = (clifd > s_srv_ctx->maxfd ? clifd : s_srv_ctx->maxfd);
        }

        retval = select(s_srv_ctx->maxfd + 1, readfds, NULL, NULL, &tv);

        if (retval < 0) {
            fprintf(stderr, "select error:%s.\n", strerror(errno));
            return -1;
        }

        if (retval == 0) {
            fprintf(stdout, "select is timeout.\n");
            continue;
        }

        if (FD_ISSET(srvfd[0], readfds)) {
            /*����TCP�ͻ���accept����*/
            accept_client_proc(srvfd[0]);
        } if (FD_ISSET(srvfd[1], readfds)) {
            /*����UDP�ͻ�������*/
            recv_udp_client_msg(srvfd[1]);
        } if (FD_ISSET(srvfd[2], readfds)) {
            /*�����ಥ�ͻ�������*/
            recv_mcast_msg(srvfd[2]);
        }else {
            /*���ܴ���ͻ�����Ϣ*/
            recv_tcp_client_msg(readfds);
        }
    }
	return 0;
}

static int accept_client_proc(int srvfd)
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    cliaddrlen = sizeof(cliaddr);
    int clifd = -1;

    printf("accpet clint proc is called.\n");

ACCEPT:
    clifd = accept(srvfd,(struct sockaddr*)&cliaddr,&cliaddrlen);

    if (clifd < 0) {
        if (errno == EINTR) {
            goto ACCEPT;
        } else {
            fprintf(stderr, "accept fail,error:%d,reason:%s\n", errno, strerror(errno));
			return -1;
        }
    }

    fprintf(stdout, "accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);

    //���µ�������������ӵ�������
    int i = 0;
    for (i = 0; i < SIZE; i++) {
        if (s_srv_ctx->clifds[i] < 0) {
            s_srv_ctx->clifds[i] = clifd;
            s_srv_ctx->cli_cnt++;
            break;
        }
    }

    if (i == SIZE) {
        fprintf(stderr,"too many clients.\n");
        return -2;
    }
	return 0;
}

static int recv_udp_client_msg(int srvfd)
{
    int i = 0, n = 0;
	char buf[MAXLINE] = {0};
	struct sockaddr_in clientAddr;  
    socklen_t len = sizeof(clientAddr); 
	
	if(recvfrom(srvfd, buf, MAXLINE, 0, (struct sockaddr*) & clientAddr, &len) < 0) {
		fprintf(stderr, "recvfrom fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1;
	}
	
    handle_udp_client_msg(srvfd, clientAddr, buf);
	return 0;
}

static int recv_tcp_client_msg(fd_set *readfds)
{
    int i = 0, n = 0;
    int clifd;
    char buf[MAXLINE] = {0};
    for (i = 0;i <= s_srv_ctx->cli_cnt;i++) {
        clifd = s_srv_ctx->clifds[i];
        if (clifd < 0) {
            continue;
        }

        if (FD_ISSET(clifd, readfds)) {
            //���տͻ��˷��͵���Ϣ
            n = read(clifd, buf, MAXLINE);
            if (n <= 0) {
                FD_CLR(clifd, &s_srv_ctx->allfds);
                close(clifd);
                s_srv_ctx->clifds[i] = -1;
                continue;
            }

            handle_tcp_client_msg(clifd, buf);
        }
    }
	return 0;
}

static int recv_mcast_msg(int srvfd)
{
    int i = 0, n = 0;
	char buf[MAXLINE] = {0};
	struct sockaddr_in clientAddr;  
    socklen_t len = sizeof(clientAddr); 
	
	if(recvfrom(srvfd, buf, MAXLINE, 0, (struct sockaddr*) & clientAddr, &len) < 0) {
		fprintf(stderr, "recvfrom fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1;
	}
	
    handle_mcast_msg(srvfd, clientAddr, buf);
}

static int handle_tcp_client_msg(int fd, char *buf) 
{
	int n;
    assert(buf);

    printf("recv buf is :%s\n", buf);

    n = write(fd, buf, strlen(buf) +1);
	if (n < 0) {  
		fprintf(stderr, "write fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1; 
	}  

    return 0;
}

static int handle_udp_client_msg(int fd, struct sockaddr_in addr, char *buf) 
{
	int n;
    assert(buf);
    printf("recv buf is :%s\n", buf);

    n = sendto(fd, buf, strlen(buf) +1, 0, (struct sockaddr *) &addr, sizeof(addr));
	if (n < 0) {  
		fprintf(stderr, "sendto fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1;  
	}  
    return 0;
}

static int handle_mcast_msg(int fd, struct sockaddr_in addr, char *buf)
{
	int n;
    assert(buf);
	
	if (n < 0) {  
		fprintf(stderr, "sendto fail,error:%d,reason:%s\n", errno, strerror(errno));
		return -1;  
	}  
	fprintf(stdout, "recv a new msg from: %s:%d\n", inet_ntoa(addr.sin_addr), addr.sin_port);
    printf("recv buf is :%s\n", buf);
    return 0;
}

static void server_uninit()
{
    if (s_srv_ctx) {
        free(s_srv_ctx);
        s_srv_ctx = NULL;
    }
}

static int server_init()
{
    s_srv_ctx = (server_context_st *)malloc(sizeof(server_context_st));
    if (s_srv_ctx == NULL) {
        return -1;
    }

    memset(s_srv_ctx, 0, sizeof(server_context_st));

    int i = 0;
    for (;i < SIZE; i++) {
        s_srv_ctx->clifds[i] = -1;
    }

    return 0;
}

int main(int argc,char *argv[])
{
    int srvfd[3];

    if (server_init() < 0) {
        return -1;
    }

    srvfd[0] = create_server_tcp_proc(IPADDR, TCP_PORT);
	srvfd[1] = create_server_udp_proc(IPADDR, UDP_PORT);
	srvfd[2] = create_mcast_proc(MCASTADDR, MCASTPORT);
	
    if ((srvfd[0] || srvfd[1] || srvfd[2]) < 0) {
        fprintf(stderr, "socket create or bind fail.\n");
        goto err;
    }

    handle_client_proc(srvfd);

    return 0;

err:
    server_uninit();
    return -1;
}
/*================================ File End =========================================*/