#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <pthread.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <syslog.h>

#include "ikcp.h"
#include "kcp_util.h"
#include "kcp_config.h"
#include "commandline.h"
#include "kcp_client.h"
#include "debug.h"


//int total_send_bytes=0;

ikcpcb *kcp1=NULL;  // global var , just for easy use


void timer_event_cb(evutil_socket_t fd, short event, void *arg)
{
	ikcp_update(kcp1, iclock());
	set_timer_interval(arg,0,kcp_get_param()->interval);
}


static int timer_count=0;
static void print_result_timer_event_cb(evutil_socket_t fd, short event, void *arg)
{
    if(udp_send_band >0){
	  timer_count++;
	  printf("%d udp_send_band :%d %d %d\n ", timer_count,udp_send_band,kcp1->nsnd_que, kcp1->nsnd_buf);
    }
	set_timer_interval(arg,1,0);
	//udp_send_band=0;
}


// run every time to control the data rate, no pacing 
static void write_event_cb(evutil_socket_t fd, short event, void *arg)
{
    char buf[2000] = {0};
	int  len=1500, nret;
	int count=10;
	while ( count > 0) {
		count--;
		nret = ikcp_send(kcp1, buf, len);
		if(nret <0){
			printf("ikcp_send error %d\n", nret);
			return;
		}
		memset(buf, 0, 2000);
	}
	set_timer_interval(arg, 1, 0);
}


extern long count;
extern int average_send_bytes;

static int set_xkcp_listener()
{
	short lport = kcp_get_param()->local_port;
	struct sockaddr_in sin;
	char *addr = get_iface_ip(kcp_get_param()->local_interface);
	if (!addr) {
		debug(LOG_ERR, "get_iface_ip [%s] failed", kcp_get_param()->local_interface);
		exit(0);
	}
	
	memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(addr);
    sin.sin_port = htons(lport);
	
	int xkcp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (bind(xkcp_fd, (struct sockaddr *) &sin, sizeof(sin))) {
		debug(LOG_ERR, "xkcp_fd bind() failed %s ", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	return xkcp_fd;
}


static void kcp_rcv_cb(const int sock, short int which, void *arg)
{	
	struct sockaddr_in clientaddr;
	int clientlen = sizeof(clientaddr);
	memset(&clientaddr, 0, clientlen);
	
	char buf[BUF_RECV_LEN] = {0};
	int len = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &clientaddr, (socklen_t*)&clientlen);
	if (len > 0) {
		//printf("len %d\n",len);
		//udp_recv_band += len;
		//accept_client_data(sock, base, &clientaddr, clientlen, buf, len);
		
		int nret = ikcp_input(kcp1, buf, len);
		
		if (nret < 0) {
			debug(LOG_ERR, "[%d] ikcp_input failed [%d]", kcp1->conv, nret);
		}	
			
		while(1) {
				char obuf[OBUF_SIZE] = {0};
				int nrecv = ikcp_recv(kcp1, obuf, OBUF_SIZE);
				if (nrecv < 0) {
					if (nrecv == -3)
						debug(LOG_INFO, "obuf is small, need to extend it");
					break;
				}
					//printf("nrecv %d\n",nrecv);
				//ikcp_recv_band += nrecv;
					//printf("rcv(buf=%d, queue=%d)\n", kcp->nrcv_buf, kcp->nrcv_que);
					//printf("kcp %d\n", ikcp_recv_band);
		}
	    
	}	
}


int client_main_loop(void)
{
	struct event_base *base = NULL;
	struct evconnlistener *listener = NULL, *mlistener = NULL;
	int xkcp_fd = set_xkcp_listener();
	struct event timer_event, print_result_event,write_event,*xkcp_event = NULL;;

	if (xkcp_fd < 0) {
		debug(LOG_ERR, "ERROR, open udp socket");
		exit(0);
	}

	struct hostent *server = gethostbyname(kcp_get_param()->remote_addr);
	if (!server) {
		debug(LOG_ERR, "ERROR, no such host as %s", kcp_get_param()->remote_addr);
		exit(0);
	}

	base = event_base_new();
	if (!base) {
		debug(LOG_ERR, "event_base_new()");
		exit(0);
	}

	struct kcp_proxy_param  proxy_param;
	memset(&proxy_param, 0, sizeof(proxy_param));
	proxy_param.base 		= base;
	proxy_param.xkcpfd 		= xkcp_fd;
	proxy_param.sockaddr.sin_family 	= AF_INET;
	proxy_param.sockaddr.sin_port		= htons(kcp_get_param()->remote_port);
	memcpy((char *)&proxy_param.sockaddr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);

	kcp1 = ikcp_create(KCP_CON_ID, &proxy_param); 
	kcp_set_config_param(kcp1);

	event_assign(&timer_event, base, -1, EV_PERSIST, timer_event_cb, &timer_event);
	set_timer_interval(&timer_event,0,kcp_get_param()->interval);

	event_assign(&print_result_event, base, -1, EV_PERSIST, print_result_timer_event_cb, &print_result_event);
	set_timer_interval(&print_result_event,1,0);

	event_assign(&write_event, base, -1, EV_PERSIST, write_event_cb, &write_event);
	set_timer_interval(&write_event,1,0);

	xkcp_event = event_new(base, xkcp_fd, EV_READ|EV_PERSIST, kcp_rcv_cb, base);
	event_add(xkcp_event, NULL);

	event_base_dispatch(base);
	evconnlistener_free(mlistener);
	evconnlistener_free(listener);
	close(xkcp_fd);
	event_base_free(base);

   
	return 0;
}

int main(int argc, char **argv)
{
	struct kcp_config *config = kcp_get_config();
	config->main_loop = client_main_loop;

	return kcp_main(argc, argv);
}
