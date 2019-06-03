#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
 
static char *getip()
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	char *host = NULL;
 
	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return NULL;
	}
 
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
 
		family = ifa->ifa_addr->sa_family;
 
		if (!strcmp(ifa->ifa_name, "lo"))
			continue;
		if (family == AF_INET) {
			if ((host = malloc(NI_MAXHOST)) == NULL)
				return NULL;
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
					host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0) {
				return NULL;
			}
			freeifaddrs(ifaddr);
			return host;
		}
	}
	return NULL;
}
