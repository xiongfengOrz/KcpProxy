// this config file is imitate the config of kcptun, but only some config is ok, 
// others like fec is not ok !!!


#ifndef	_KCP_CONFIG_
#define	_KCP_CONFIG_

struct kcp_param {
	char	*local_interface; 	// localaddr
	char	*remote_addr; 	// remoteaddr
	char	*key;			// key
	char	*crypt;			// crypt
	char	*mode;			// mode
	int		local_port;		// local tcp listen port
	int		remote_port;	// remote udp connect port
	int 	conn;			// conn
	int 	auto_expire;	// autoexpire
	int 	scavenge_ttl;	// scavengettl
	int		mtu;			// mtu
	int		sndwnd;			// sndwnd
	int		rcvwnd;			// rcvwnd
	int		data_shard;		// datashard
	int		parity_shard;  	// parityshard
	int		dscp;			// dscp
	int 	nocomp; 		// nocomp
	int		ack_nodelay;	// acknodelay
	int 	nodelay;		// nodelay
	int		interval;		// interval
	int 	resend;			// resend
	int 	nc; 			// no congestion
	int 	sock_buf;		// sockbuf
	int 	keepalive;		// keepalive
};

struct kcp_config {
	char 	*config_file;
	int 	daemon;
	int		is_server;
	int		(*main_loop)();

	struct kcp_param param;
};

void config_init(void);

struct kcp_config *kcp_get_config(void);

int kcp_param_validate(struct kcp_param * param);

int kcp_parse_param(const char *filename);

int kcp_parse_json_param(struct kcp_param *config, const char *filename);

struct kcp_param *kcp_get_param(void);

#endif
