// CAP_NET_RAW or root is required for this program to work correctly

// Adapted: https://gist.github.com/austinmarton/1922600

#include <arpa/inet.h>       // inet_ntoa()
#include <errno.h>           // errno
#include <linux/if_packet.h> // struct sockaddr_ll
#include <net/if.h>          // struct ifreq
#include <netinet/ether.h>   // struct ether_header
#include <netinet/ip.h>      // struct iphdr
#include <signal.h>          // sigaction()
#include <stdbool.h>         // bool
#include <stdio.h>           // printf()
#include <stdlib.h>          // exit()
#include <string.h>          // memset()
#include <sys/ioctl.h>       // ioctl()
#include <sys/socket.h>      // socket()
#include <sys/types.h>       // socket()
#include <unistd.h>          // usleep()

#define msleep(X) usleep(1000 * (X))

#define ETH_FRAME_SIZE 2048 // (1518) to big // 1522 // 1514 ok

#define IPv4_ONLY 1

static uint8_t gateway_mac[] = {0xcc, 0xce, 0x1e, 0x3a, 0x40, 0xe8};

static uint8_t interface_mac[6];

static int rcv_sock = -1;
static int fwd_sock = -1;

static void die(const char *msg) {
	if (errno) {
		fprintf(stderr, "[EXIT %d] (%s) %s\n", errno, strerror(errno), msg);
	} else {
		fprintf(stderr, "[EXIT] %s\n", msg);
	}

	exit(EXIT_FAILURE);
}

static void close_sockets(int signum) {
	(void) signum;

	if (rcv_sock != -1) {
		close(rcv_sock);
	}
	if (fwd_sock != -1) {
		close(fwd_sock);
	}
}

static void get_gateway_mac() {
	// TODO
}

static void init(const char *network_interface, struct sockaddr_ll *sadr) {
	struct sigaction ign = {
		.sa_handler = SIG_IGN,
		.sa_flags   = SA_RESTART,
	};
	sigemptyset(&ign.sa_mask);
	sigaction(SIGPIPE, &ign, NULL);

	struct sigaction term = {
		.sa_handler = &close_sockets,
	};
	sigemptyset(&term.sa_mask);
	sigaction(SIGTERM, &term, NULL);
	sigaction(SIGINT, &term, NULL);

	// ETH_P_IP:   IPv4 only
	// ETH_P_IPV6: IPv6 only
	// ETH_P_ALL:  All Frames
#ifdef IPv4_ONLY
	rcv_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
#else
	rcv_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
#endif
	if (rcv_sock == -1) {
		die("socket(2)");
	}

	// IPPROTO_RAW:
	fwd_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fwd_sock == -1) {
		die("socket(2)");
	}

	// Bind to given network interface
	struct ifreq ifr = {0};
	strncpy(ifr.ifr_name, network_interface, IFNAMSIZ - 1);
	if (ioctl(fwd_sock, SIOCGIFINDEX, &ifr) == -1) {
		die("ioctl(2)");
	}

	// Bind to given network interface
	// and enable promiscuous mode (receive all packets)
	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl(rcv_sock, SIOCGIFINDEX, &ifr) == -1) {
		die("ioctl(2)");
	}

	// Collect network interface metadata
	struct ifreq if_idx = {0};
	struct ifreq if_mac = {0};

	// Get the index of the network interface to send on
	strncpy(if_idx.ifr_name, network_interface, IFNAMSIZ - 1);
	if (ioctl(fwd_sock, SIOCGIFINDEX, &if_idx) == -1) {
		die("ioctl(2)");
	}
	sadr->sll_ifindex = if_idx.ifr_ifindex;

	// Get the MAC address of the network interface to send on
	strncpy(if_mac.ifr_name, network_interface, IFNAMSIZ - 1);
	if (ioctl(fwd_sock, SIOCGIFHWADDR, &if_mac) == -1) {
		die("ioctl(2)");
	}
	memcpy(interface_mac, &if_mac.ifr_hwaddr.sa_data, 6);
	memcpy(sadr->sll_addr, gateway_mac, 6);

	sadr->sll_halen = ETH_ALEN;
}

static inline ssize_t receive_eth_frame(struct ether_header *eh, size_t len) {
	ssize_t packet_size = recvfrom(rcv_sock, eh, len, 0, NULL, NULL);
	if (packet_size == -1) {
		die("recvfrom(2)");
	}
	return packet_size;
}

static inline void send_eth_frame(struct ether_header *eh, size_t len, const struct sockaddr_ll *sadr) {
	// Set destination MAC address
	memcpy(eh->ether_shost, interface_mac, 6);
	memcpy(eh->ether_dhost, gateway_mac, 6);

	if (sendto(fwd_sock, eh, len, 0, (struct sockaddr *) sadr, sizeof(struct sockaddr_ll)) == -1) {
		if (errno == EMSGSIZE) {
			// Probably a corrupt package or MTU to small
			// So... Ignore that :)
			return;
		}
		die("sendto(2)");
	}
}

static void print_information(const struct ether_header *eh, size_t len) {
	static size_t internal_counter = 0;

	printf("Software Counter: %lu\n", internal_counter++);
	printf("Eth Frame Size (bytes): %lu\n", len);

#ifndef IPv4_ONLY
	if (eh->ether_type == htons(0x86dd)) { // IPv6 (0x86dd)
		printf("IPv6 - Skipping...\n");
		return;
	}

	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		printf("Unknown ether type - Skipping...\n");
		return;
	}
#endif

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

static inline bool is_valid_frame(const struct ether_header *eh, const char *fwd_address) {
#ifndef IPv4_ONLY
	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		return false;
	}
#endif

	struct iphdr *ip_packet                  = (struct iphdr *) (eh + 1);
	struct sockaddr_in source_socket_address = {0};
	source_socket_address.sin_addr.s_addr    = ip_packet->saddr;

	// Surely there are better ways to solve this, but I don't care at the moment...
	return (strcmp((char *) inet_ntoa(source_socket_address.sin_addr), fwd_address) == 0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Missing argument!\n");
		exit(EXIT_FAILURE);
	}

	get_gateway_mac();

	struct sockaddr_ll sadr = {0};
	init(argv[1], &sadr);

	struct ether_header frame_data[ETH_FRAME_SIZE];
	size_t recv_size;

	while (1) {
		printf("\n");

		recv_size = (size_t) receive_eth_frame(frame_data, ETH_FRAME_SIZE);

		print_information(frame_data, recv_size);

		if (!is_valid_frame(frame_data, argv[2])) {
			continue;
		}

		print_mac_addresses(frame_data);

		send_eth_frame(frame_data, recv_size, &sadr);

		print_mac_addresses(frame_data);

		msleep(10);
	}

	return EXIT_SUCCESS;
}
