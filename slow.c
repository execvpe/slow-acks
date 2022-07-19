// CAP_NET_RAW or root is required for this program to work correctly

// Adapted: https://gist.github.com/austinmarton/1922600

#include <arpa/inet.h>       // inet_ntoa()
#include <errno.h>           // errno
#include <linux/if_packet.h> // struct sockaddr_ll
#include <net/if.h>          // struct ifreq
#include <netinet/ether.h>   // struct ether_header
#include <netinet/ip.h>      // struct iphdr
#include <stdbool.h>         // bool
#include <stdio.h>           // printf()
#include <stdlib.h>          // exit()
#include <string.h>          // memset()
#include <sys/ioctl.h>       // ioctl()
#include <sys/socket.h>      // socket()
#include <sys/types.h>       // socket()
#include <unistd.h>          // usleep()

#define msleep(X) usleep(1000 * (X))

#define ETH_FRAME_SIZE (1518) // 1522

const unsigned char gateway_mac[] = {0xcc, 0xce, 0x1e, 0x3a, 0x40, 0xe8};
const unsigned char this_mac[]    = {0xa8, 0x7e, 0xea, 0x45, 0x13, 0x20};

static const char *if_name  = NULL;
static const char *fwd_addr = NULL;

static struct ether_header frame_data[ETH_FRAME_SIZE];

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

static void get_mac_addresses() {
	// TODO
}

static void open_sockets(int *rcv_sock, int *fwd_sock) {
	// ETH_P_IP:   only IPv4
	// ETH_P_IPV6: only IPv6
	// ETH_P_ALL:  all frames
	*rcv_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));

	if (*rcv_sock == -1) {
		die("socket(2)");
	}

	// Enable promiscuous mode (receive all packets)
	// and bind to given interface

	struct ifreq ifr = {0};
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
	ifr.ifr_flags |= IFF_PROMISC;

	ioctl(*rcv_sock, SIOCGIFINDEX, &ifr);

	*fwd_sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);

	if (*fwd_sock == -1) {
		die("socket(2)");
	}
}

static ssize_t receive_eth_frame(int rcv_sock, struct ether_header *eh, size_t len) {
	ssize_t packet_size = recvfrom(rcv_sock, eh, len, 0, NULL, NULL);
	if (packet_size == -1) {
		die("recvfrom");
	}
	return packet_size;
}

static void send_eth_frame(int fwd_sock, struct ether_header *eh, size_t len) {
	static bool init               = false;
	static struct ifreq if_idx     = {0};
	static struct ifreq if_mac     = {0};
	static struct sockaddr_ll sadr = {0};

	if (!init) {
		// Get the index of the interface to send on
		strncpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
		if (ioctl(fwd_sock, SIOCGIFINDEX, &if_idx) < 0) {
			die("ioctl(2)");
		}

		// Get the MAC address of the interface to send on
		strncpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
		if (ioctl(fwd_sock, SIOCGIFHWADDR, &if_mac) < 0) {
			die("ioctl(2)");
		}

		sadr.sll_ifindex = if_idx.ifr_ifindex;
		sadr.sll_halen   = ETH_ALEN;

		init = true;
	}

	// Set destination MAC address
	for (int i = 0; i < 6; i++) {
		eh->ether_shost[i] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[i];
		eh->ether_dhost[i] = gateway_mac[i];
		sadr.sll_addr[i]   = gateway_mac[i];
	}

	if (sendto(fwd_sock, eh, len, 0, (struct sockaddr *) &sadr, sizeof(struct sockaddr_ll)) == -1) {
		if (errno == EMSGSIZE) {
			// Ignore that :)
			return;
		}
		die("sendto(2)");
	}
}

static void print_information(const struct ether_header *eh, size_t len) {
	printf("Eth Frame Size (bytes): %lu\n", len);

	//	if (eh->ether_type == htons(0x86dd)) { // IPv6 (0x86dd)
	//		printf("IPv6 - Skipping...\n");
	//		return;
	// }

	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		printf("Unknown ether type - Skipping...\n");
		return;
	}

	struct iphdr *ip_packet = (struct iphdr *) (eh + 1);
	printf("IP Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));

	struct sockaddr_in socket_address = {0};
	socket_address.sin_addr.s_addr    = ip_packet->saddr;
	printf("Source Address: %s\n", (char *) inet_ntoa(socket_address.sin_addr));

	socket_address.sin_addr.s_addr = ip_packet->daddr;
	printf("Destination Address: %s\n", (char *) inet_ntoa(socket_address.sin_addr));

	printf("Identification: %d\n", ntohs(ip_packet->id));
}

static void print_mac_addresses(const struct ether_header *eh) {
	printf("[Rcv] Src MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2],
		   eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5]);
	printf("[Rcv] Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
		   eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);
}

static bool is_valid_frame(const struct ether_header *eh) {
	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		return false;
	}

	struct iphdr *ip_packet                  = (struct iphdr *) (eh + 1);
	struct sockaddr_in source_socket_address = {0};
	source_socket_address.sin_addr.s_addr    = ip_packet->saddr;

	return (strcmp((char *) inet_ntoa(source_socket_address.sin_addr), fwd_addr) == 0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Missing argument!\n");
		exit(EXIT_FAILURE);
	}

	if_name  = argv[1];
	fwd_addr = argv[2];

	get_mac_addresses();

	int rcv_sock = -1;
	int fwd_sock = -1;
	open_sockets(&rcv_sock, &fwd_sock);

	while (1) {
		printf("\n");

		size_t recv_size = (size_t) receive_eth_frame(rcv_sock, frame_data, ETH_FRAME_SIZE);

		print_information(frame_data, recv_size);

		if (!is_valid_frame(frame_data)) {
			continue;
		}

		print_mac_addresses(frame_data);

		send_eth_frame(fwd_sock, frame_data, recv_size);

		print_mac_addresses(frame_data);

		msleep(10);
	}

	return EXIT_SUCCESS;
}
