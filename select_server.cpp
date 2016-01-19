/*============================================================================
File Name:select_server.cpp
Purples:学习select机制

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


/*=============================== 全局定义 =====================================*/
typedef struct server_context_st
{
    int cli_cnt;        /*客户端个数*/
    int clifds[SIZE];   /*客户端个数*/
    fd_set allfds;      /*句柄集合*/
    int maxfd;          /*句柄最大值*/
} server_context_st;
/*============================================================================*/

/*=============================== 全局变量 =====================================*/
static server_context_st *s_srv_ctx = NULL;
/*============================================================================*/

/*=============================== 函数声明 =====================================*/
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

/*=============================== 函数实现 =====================================*/  
static int create_server_tcp_proc(const char* ip,int port)
{
    int  fd;
    struct sockaddr_in servaddr;
    fd = socket(AF_INET, SOCK_STREAM,0);
    if (fd  < 0) {
		fprintf(stderr, "create socket fail,error:%d,reason:%s\n", errno, strerror(errno));
        return -1;
    }

	//设置套接字选项，允许重用本地地址和端口
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
	//设置套接字选项，允许重用本地地址和端口
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
    
	/*初始化地址*/
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port        = htons(port);
   
    if(bind(fd, (struct sockaddr*) &local_addr, sizeof(local_addr)) < 0)
    {
        fprintf(stderr, "socket bind fail,erron:%d,reason:%s\n", errno, strerror(errno));
        return -2;
    }

	/*加入多播组*/
	mreq.imr_multiaddr.s_addr = inet_addr(ip); /*多播地址设置*/
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);/*网络接口为默认*/
	
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

        /*每次调用select前都要重新设置文件描述符和时间，因为事件发生后，文件描述符和时间都被内核修改啦*/
        /*添加监听套接字*/
        FD_ZERO(readfds);
        FD_SET(srvfd[0], readfds);
        s_srv_ctx->maxfd = srvfd[0];
		
		FD_SET(srvfd[1], readfds);
        s_srv_ctx->maxfd = srvfd[1];
		
		FD_SET(srvfd[2], readfds);
        s_srv_ctx->maxfd = srvfd[2];

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        /*添加客户端套接字*/
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
            /*监听TCP客户端accept请求*/
            accept_client_proc(srvfd[0]);
        } if (FD_ISSET(srvfd[1], readfds)) {
            /*监听UDP客户端数据*/
            recv_udp_client_msg(srvfd[1]);
        } if (FD_ISSET(srvfd[2], readfds)) {
            /*监听多播客户端数据*/
            recv_mcast_msg(srvfd[2]);
        }else {
            /*接受处理客户端消息*/
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

    //将新的连接描述符添加到数组中
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
            //接收客户端发送的信息
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