// Source: https://www.binarytides.com/raw-sockets-c-code-linux/

// USE CAP_NET_RAW

#include <arpa/inet.h> // inet_addr
#include <errno.h>     //For errno - the error number
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/ip.h>  //Provides declarations for ip header
#include <netinet/tcp.h> //Provides declarations for tcp header
#include <stdio.h>       //for printf
#include <stdlib.h>      //for exit(0);
#include <string.h>      //memset
#include <sys/ioctl.h>
#include <sys/socket.h> //for socket ofcourse
#include <sys/types.h>
#include <unistd.h> // sleep()

#define INTERFACE "wlp0s20f3"

#define ETH_FRAME_SIZE (1522)

const unsigned char gateway_mac[] = {0xcc, 0xce, 0x1e, 0x3a, 0x40, 0xe8};
const unsigned char this_mac[]    = {0xa8, 0x7e, 0xea, 0x45, 0x13, 0x20};

static int eth_sock = -1;
static int fwd_sock = -1;
static char payload[ETH_FRAME_SIZE];

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

static void get_gateway_mac() {
	// TODO
}

static void create_sock() {
	eth_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));

	if (eth_sock == -1) {
		die("socket(2)");
	}

	// Enable promiscuous mode (receive all packets)
	// Bind to wifi interface

	struct ifreq ifr = {0};
	strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
	ifr.ifr_flags |= IFF_PROMISC;

	ioctl(eth_sock, SIOCGIFINDEX, &ifr);

	fwd_sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);

	if (fwd_sock == -1) {
		die("socket(2)");
	}
}

ssize_t receive_eth_frame(char *buf, size_t len) {
	ssize_t packet_size = recvfrom(eth_sock, buf, len, 0, NULL, NULL);
	if (packet_size == -1) {
		die("recvfrom");
	}
	return packet_size;
}

void send_eth_frame(char *buf, size_t len) {
	// Get the index of the interface to send on
	struct ifreq if_idx = {0};
	strncpy(if_idx.ifr_name, INTERFACE, IFNAMSIZ - 1);
	if (ioctl(fwd_sock, SIOCGIFINDEX, &if_idx) < 0) {
		die("ioctl(2)");
	}

	struct sockaddr_ll sadr = {.sll_ifindex = if_idx.ifr_ifindex, .sll_halen = ETH_ALEN};

	// Set destination MAC address
	struct ether_header *eh = (struct ether_header *) buf;
	for (int i = 0; i < 6; i++) {
		eh->ether_shost[i] = this_mac[i];
		eh->ether_dhost[i] = gateway_mac[i];
		sadr.sll_addr[i]   = gateway_mac[i];
	}
	// eh->ether_type = htons(ETH_P_IP);

	if (sendto(fwd_sock, buf, len, 0, (struct sockaddr *) &sadr, sizeof(struct sockaddr_ll)) == -1) {
		if (errno == EMSGSIZE) {
			// Ignore that :)
			return;
		}
		die("sendto(2)");
	}
}

void main(void) {
	create_sock();

	while (1) {
		size_t packet_size = receive_eth_frame(payload, ETH_FRAME_SIZE);
		char *buf          = payload;

		buf += 14; // Drop ethernet frame

		struct sockaddr_in source_socket_address, dest_socket_address;
		struct iphdr *ip_packet = (struct iphdr *) buf;

		memset(&source_socket_address, 0, sizeof(source_socket_address));
		source_socket_address.sin_addr.s_addr = ip_packet->saddr;
		memset(&dest_socket_address, 0, sizeof(dest_socket_address));
		dest_socket_address.sin_addr.s_addr = ip_packet->daddr;

		if (strcmp((char *) inet_ntoa(source_socket_address.sin_addr), "172.28.70.2") != 0) {
			continue;
		}

		printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
		printf("Frame Size (bytes): %d\n", packet_size);
		printf("Source Address: %s\n", (char *) inet_ntoa(source_socket_address.sin_addr));
		printf("Destination Address: %s\n", (char *) inet_ntoa(dest_socket_address.sin_addr));
		printf("Identification: %d\n", ntohs(ip_packet->id));

		unsigned char *ethhead = payload;
		printf("[Rcv] Src MAC address: "
			   "%02x:%02x:%02x:%02x:%02x:%02x\n",
			   ethhead[6], ethhead[7], ethhead[8], ethhead[9], ethhead[10], ethhead[11]);
		printf("[Rcv] Dst MAC address: "
			   "%02x:%02x:%02x:%02x:%02x:%02x\n",
			   ethhead[0], ethhead[1], ethhead[2], ethhead[3], ethhead[4], ethhead[5]);

		send_eth_frame(payload, packet_size);

		printf("[Fwd] Src MAC address: "
			   "%02x:%02x:%02x:%02x:%02x:%02x\n",
			   ethhead[6], ethhead[7], ethhead[8], ethhead[9], ethhead[10], ethhead[11]);
		printf("[Fwd] Dst MAC address: "
			   "%02x:%02x:%02x:%02x:%02x:%02x\n\n",
			   ethhead[0], ethhead[1], ethhead[2], ethhead[3], ethhead[4], ethhead[5]);
	}
}
