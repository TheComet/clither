#include "clither/net.h"

#include <emscripten/websocket.h>

int
net_init(void)
{
	return 0;
}

void
net_deinit(void)
{}

void
net_log_host_ips(void)
{}

void
net_addr_to_str(char* str, int len, const void* addr)
{
	str[0] = '\0';
}

int
net_bind(const char* bind_address, const char* port, int* addrlen)
{
	return -1;
}

int
net_connect(struct cs_vector* sockfds, const char* server_address, const char* port)
{
	return -1;
}

void
net_close(int sockfd)
{
}

int
net_sendto(int sockfd, const void* buf, int len, const void* addr, int addrlen)
{
	return -1;
}

int
net_send(int sockfd, const void* buf, int len)
{
	return -1;
}

int
net_recvfrom(int sockfd, void* buf, int capacity, void* addr, int addrlen)
{
	return -1;
}

int
net_recv(int sockfd, void* buf, int capacity)
{
	return -1;
}

