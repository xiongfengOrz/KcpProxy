#ifndef	_KCP_UTIL_
#define	_KCP_UTIL_

#include "ikcp.h"

#define HTTP_IP_ADDR_LEN	16
#define	OBUF_SIZE 			4096
#define	BUF_RECV_LEN		1500
#define KCP_CON_ID 			0x11223344

struct event;
struct eventbase;
struct sockaddr_in;
struct bufferevent;

extern int udp_send_band;
extern int total_ikcp_recv ;
extern int ikcp_recv_band;
extern int recv_queue;
extern int recv_buf ;

struct kcp_proxy_param {
	struct event_base 	*base;
	int 				xkcpfd;
	struct sockaddr_in	sockaddr;
	int 				addr_len;
};


void itimeofday(long *sec, long *usec);

IINT64 iclock64(void);

IUINT32 iclock();

char *get_iface_ip(const char *ifname);


void kcp_set_config_param(ikcpcb *kcp);

void set_no_delay(evutil_socket_t fd);


void set_timer_interval(struct event *timeout,int second, int milltime);

int kcp_main(int argc, char **argv);

#endif
