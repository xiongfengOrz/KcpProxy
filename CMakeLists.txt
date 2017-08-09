cmake_minimum_required(VERSION 2.6)

project(kcptun C)


set(src_kcp_server
	kcp_server.c
	debug.c
	ikcp.c
	commandline.c
	kcp_util.c
	kcp_config.c
        json.c
	)
	
set(src_kcp_client
	kcp_client.c
	kcp_config.c
	kcp_util.c
	commandline.c
	debug.c
	json.c
	ikcp.c
	)

set(libs
    m
	event)

ADD_DEFINITIONS(-Wall -O2 --std=gnu99 -Wmissing-declarations)

add_executable(kcp_client ${src_kcp_client})
target_link_libraries(kcp_client ${link_flag} ${libs})

add_executable(kcp_server ${src_kcp_server})
target_link_libraries(kcp_server ${link_flag} ${libs})

install(TARGETS kcp_client kcp_server
        RUNTIME DESTINATION bin
)
