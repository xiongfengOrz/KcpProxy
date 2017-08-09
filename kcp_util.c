#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>		 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>

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
#include "debug.h"


void itimeofday(long *sec, long *usec)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

char *get_iface_ip(const char *ifname)
{
	struct ifreq if_data;
	struct in_addr in;
	char *ip_str;
	int sockd;
	u_int32_t ip;

	if ((sockd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		debug(LOG_ERR, "socket(): %s", strerror(errno));
		return NULL;
	}
	strncpy(if_data.ifr_name, ifname, 15);
	if_data.ifr_name[15] = '\0';

	if (ioctl(sockd, SIOCGIFADDR, &if_data) < 0) {
		debug(LOG_ERR, "ioctl(): SIOCGIFADDR %s", strerror(errno));
		close(sockd);
		return NULL;
	}
	memcpy((void *)&ip, (void *)&if_data.ifr_addr.sa_data + 2, 4);
	in.s_addr = ip;

	close(sockd);
	ip_str = malloc(HTTP_IP_ADDR_LEN);
	memset(ip_str, 0, HTTP_IP_ADDR_LEN);
	if(ip_str&&inet_ntop(AF_INET, &in, ip_str, HTTP_IP_ADDR_LEN))
		return ip_str;

	if (ip_str) free(ip_str);
	return NULL;
}



int udp_send_band=0;

double  average_send_bytes =0.0;


static int xkcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	struct kcp_proxy_param *ptr = user;
	//ptr->sockaddr.sin_addr.s_addr = inet_addr("192.168.1.104");
	int nret = sendto(ptr->xkcpfd, buf, len, 0, (struct sockaddr *)&ptr->sockaddr, sizeof(ptr->sockaddr));
	if (nret > 0){
		//printf("xkcp_output conv [%d] fd [%d] len [%d] port[%d], send datagram len %d\n",
		  	//kcp->conv, ptr->xkcpfd, len, ptr->sockaddr.sin_port, nret);
	    udp_send_band += nret;
	}
	else
		printf("xkcp_output conv [%d] fd [%d] send datagram error: (%s)\n",
		  	kcp->conv, ptr->xkcpfd, strerror(errno));

	return nret;
}

void kcp_set_config_param(ikcpcb *kcp)
{
	struct kcp_param *param = kcp_get_param();
	kcp->output	= xkcp_output;
	ikcp_wndsize(kcp, param->sndwnd, param->rcvwnd);
	ikcp_nodelay(kcp, param->nodelay, param->interval, param->resend, param->nc);
	//printf( "sndwnd [%d] rcvwnd [%d] nodelay [%d] interval [%d] resend [%d] nc [%d]",
		 //param->sndwnd, param->rcvwnd, param->nodelay, param->interval, param->resend, param->nc);
}

void set_no_delay(evutil_socket_t fd)
{
  	int one = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}



int total_ikcp_recv =0;
int ikcp_recv_band =0;

int recv_queue =0;
int recv_buf =0 ;



int kcp_main(int argc, char **argv)
{
	struct kcp_config *config = kcp_get_config();

	config_init();

	parse_commandline(argc, argv);

	if (config->main_loop == NULL) {
		debug(LOG_ERR, "should set main_loop firstly");
		exit(0);
	}

	if (config->daemon) {

		debug(LOG_INFO, "Forking into background");

		switch (fork()) {
		case 0:				
			setsid();
			config->main_loop();
			break;

		default:			 
			exit(0);
			break;
		}
	} else {
		config->main_loop();
	}

	return (0);			
}

void set_timer_interval(struct event *timeout,int second, int milltime)
{
	struct timeval tv;
	evutil_timerclear(&tv);
	tv.tv_sec = second;
	tv.tv_usec = milltime;
	event_add(timeout, &tv);
}

long count =0;



