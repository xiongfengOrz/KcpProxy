#ifndef	_KCP_CLIENT_
#define	_KCP_CLIENT_

#include <event2/util.h>
#include "ikcp.h"

void timer_event_cb(evutil_socket_t fd, short event, void *arg);

int kcp_main(int argc, char **argv);

int client_main_loop(void);

#endif
