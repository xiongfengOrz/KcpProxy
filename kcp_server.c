#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>

#include "ikcp.h"
#include "debug.h"
#include "kcp_server.h"
#include "kcp_config.h"
#include "kcp_util.h"

#ifndef NI_MAXHOST
#define NI_MAXHOST      1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV      32
#endif


double udp_recv_band=0;
double kcp_recv_band=0;

ikcpcb *kcp2=NULL;

static void timer_event_cb(evutil_socket_t fd, short event, void *arg)
{
	ikcp_update(kcp2, iclock());
	set_timer_interval(arg,0,kcp_get_param()->interval);
}


static int timer_count=0;
static void print_interval_timer_event_cb(evutil_socket_t fd, short event, void *arg)
{
    if(udp_recv_band >0){
	  timer_count++;
	  /*
      printf("udp_recv_band  :  %.1f\n",udp_recv_band);
	  printf("ikcp_recv_band :  %d\n",ikcp_recv_band);
      printf("manager_len  :  %d\n",manager_len);
	  printf("data_head :  %d\n",data_head);
	  */
	  printf("%d,%d,%d,%d\n",(int)udp_recv_band,ikcp_recv_band,kcp2->nrcv_que,kcp2->nrcv_buf);
      //printf("manager_len  :  %d\n",manager_len);
	  //printf("data_head :  %d\n",data_head);
	  //printf("rcv(buf=%d, queue=%d)\n", recv_buf, recv_queue);
	  /*
	  for(int i=0;i< all_task_list_len;++i){
	  	struct xkcp_task *task;
		iqueue_foreach(task, all_task_list[i], xkcp_task_type, head) {
			if (task->kcp) {
				printf("task_list:rcv(buf=%d, queue=%d)\n", task->kcp->nrcv_buf, task->kcp->nrcv_que);
				printf("task_list:send(buf=%d, queue=%d)\n", task->kcp->nsnd_buf, task->kcp->nsnd_que);
			}
		}
	  }*/
    }
	 
	set_timer_interval(arg,1,0);
	//udp_recv_band=0;
	//ikcp_recv_band=0;
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
		udp_recv_band += len;
		//accept_client_data(sock, base, &clientaddr, clientlen, buf, len);
		
		int nret = ikcp_input(kcp2, buf, len);
		
		if (nret < 0) {
			printf("[%d] ikcp_input failed [%d]", kcp2->conv, nret);
		}	
			
		while(1) {
				char obuf[OBUF_SIZE] = {0};
				int nrecv = ikcp_recv(kcp2, obuf, OBUF_SIZE);
				if (nrecv < 0) {
					if (nrecv == -3)
						debug(LOG_INFO, "obuf is small, need to extend it");
					break;
				}
					//printf("nrecv %d\n",nrecv);
				ikcp_recv_band += nrecv;
					//printf("rcv(buf=%d, queue=%d)\n", kcp->nrcv_buf, kcp->nrcv_que);
					//printf("kcp %d\n", ikcp_recv_band);
		}
	    
	}	
}

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
	printf("bind %s\n", addr);
	
	int xkcp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (bind(xkcp_fd, (struct sockaddr *) &sin, sizeof(sin))) {
		debug(LOG_ERR, "xkcp_fd bind() failed %s ", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	return xkcp_fd;
}


int server_main_loop()
{
	struct event timer_event, print_interval_timer_event,
	  			*xkcp_event = NULL;
	struct event_base *base = NULL;
	struct evconnlistener *mon_listener = NULL;    

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

	int xkcp_fd = set_xkcp_listener();

	struct kcp_proxy_param  proxy_param;
	memset(&proxy_param, 0, sizeof(proxy_param));
	proxy_param.base 		= base;
	proxy_param.xkcpfd 		= xkcp_fd;
	proxy_param.sockaddr.sin_family 	= AF_INET;
	proxy_param.sockaddr.sin_port		= htons(kcp_get_param()->remote_port);
	memcpy((char *)&proxy_param.sockaddr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);

	
	kcp2 = ikcp_create(KCP_CON_ID, &proxy_param);
	kcp_set_config_param(kcp2);
	
	xkcp_event = event_new(base, xkcp_fd, EV_READ|EV_PERSIST, kcp_rcv_cb, base);
	event_add(xkcp_event, NULL);
	
	event_assign(&timer_event, base, -1, EV_PERSIST, timer_event_cb, &timer_event);
	set_timer_interval(&timer_event,0,kcp_get_param()->interval);

	event_assign(&print_interval_timer_event, base, -1, EV_PERSIST, print_interval_timer_event_cb, &print_interval_timer_event);
	set_timer_interval(&print_interval_timer_event,1,0);

	
	event_base_dispatch(base);
	
	evconnlistener_free(mon_listener);
	close(xkcp_fd);
	event_base_free(base);
	return 0;
}

int main(int argc, char **argv) 
{
	struct kcp_config *config = kcp_get_config();
	config->main_loop = server_main_loop;
	
	return kcp_main(argc, argv);
}

