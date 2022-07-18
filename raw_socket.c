// Source: https://www.binarytides.com/raw-sockets-c-code-linux/

// USE CAP_NET_RAW

#include <arpa/inet.h> // inet_addr
#include <errno.h>     //For errno - the error number
#include <linux/if_ether.h>
#include <net/if.h>
#include <netinet/ip.h>  //Provides declarations for ip header
#include <netinet/tcp.h> //Provides declarations for tcp header
#include <stdio.h>       //for printf
#include <stdlib.h>      //for exit(0);
#include <string.h>      //memset
#include <sys/ioctl.h>
#include <sys/socket.h> //for socket ofcourse
#include <sys/types.h>
#include <unistd.h> // sleep()

#define IP4_PACKET_SIZE (2 << 16)
#define ETH_HEADER_SIZE (14)

#define PACKET_SIZE (IP4_PACKET_SIZE + ETH_HEADER_SIZE)

static char payload[PACKET_SIZE];

static int raw_sock = -1;

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

static void create_raw_sock() {
	raw_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));

	if (raw_sock == -1) {
		die("socket(2)");
	}

	// Enable promiscuous mode (receive all packets)
	// Bind to wifi interface

	struct ifreq ifr = {0};
	strncpy(ifr.ifr_name, "wlp0s20f3", IFNAMSIZ);
	ifr.ifr_flags |= IFF_PROMISC;

	ioctl(raw_sock, SIOCGIFINDEX, &ifr);
}

void receive_raw_packet() {
	ssize_t packet_size = recvfrom(raw_sock, payload, PACKET_SIZE, 0, NULL, NULL);
	if (packet_size == -1) {
		die("recvfrom");
	}
}

void main(void) {
	create_raw_sock();

	while (1) {
		receive_raw_packet();
		char *buf = payload;
		buf += 14; // Drop ethernet frame

		struct sockaddr_in source_socket_address, dest_socket_address;
		struct iphdr *ip_packet = (struct iphdr *) buf;

		memset(&source_socket_address, 0, sizeof(source_socket_address));
		source_socket_address.sin_addr.s_addr = ip_packet->saddr;
		memset(&dest_socket_address, 0, sizeof(dest_socket_address));
		dest_socket_address.sin_addr.s_addr = ip_packet->daddr;

		printf("Incoming Packet: \n");
		printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
		printf("Source Address: %s\n", (char *) inet_ntoa(source_socket_address.sin_addr));
		printf("Destination Address: %s\n", (char *) inet_ntoa(dest_socket_address.sin_addr));
		printf("Identification: %d\n\n", ntohs(ip_packet->id));
	}
}
