#include "eth_packet.h"

#include <arpa/inet.h>       // htons()
#include <errno.h>           // errno
#include <linux/if_packet.h> // struct sockaddr_ll
#include <net/if.h>          // struct ifreq
#include <netinet/ip.h>      // struct iphdr
#include <stdio.h>           // printf()
#include <stdlib.h>          // exit()
#include <string.h>          // memcpy()
#include <sys/ioctl.h>       // ioctl()
#include <unistd.h>          // close()

#define IPv4_ONLY 1

extern void die(const char *msg);

static int rcv_sock = -1;
static int fwd_sock = -1;

static uint8_t interface_mac[6];

static struct sockaddr_ll rcv_meta;

void eth_init(const char *network_interface, const uint8_t gateway_mac[6]) {
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
	rcv_meta.sll_ifindex = if_idx.ifr_ifindex;

	// Get the MAC address of the network interface to send on
	strncpy(if_mac.ifr_name, network_interface, IFNAMSIZ - 1);
	if (ioctl(fwd_sock, SIOCGIFHWADDR, &if_mac) == -1) {
		die("ioctl(2)");
	}
	memcpy(interface_mac, &if_mac.ifr_hwaddr.sa_data, 6);

	memcpy(rcv_meta.sll_addr, gateway_mac, 6);
	rcv_meta.sll_halen = ETH_ALEN;
}

void eth_deinit() {
	if (rcv_sock != -1) {
		close(rcv_sock);
	}
	if (fwd_sock != -1) {
		close(fwd_sock);
	}
}

ssize_t eth_receive_frame(struct ether_header *eh, size_t max_len) {
	ssize_t packet_size = recvfrom(rcv_sock, eh, max_len, 0, NULL, NULL);
	if (packet_size == -1) {
		die("recvfrom(2)");
	}
	return packet_size;
}

void eth_send_frame(struct ether_header *eh, size_t act_len) {
	memcpy(eh->ether_shost, interface_mac, 6);
	memcpy(eh->ether_dhost, rcv_meta.sll_addr, 6);

	if (sendto(fwd_sock, eh, act_len, 0, (struct sockaddr *) &rcv_meta, sizeof(struct sockaddr_ll)) == -1) {
		if (errno == EMSGSIZE) {
			// Probably a corrupt package or MTU to small
			// So... Ignore that :)
			return;
		}
		die("sendto(2)");
	}
}

bool ipv4_match_src_addr(const struct ether_header *eh, size_t act_len, const in_addr_t check_adr) {
#ifndef IPv4_ONLY
	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		return NULL;
	}
#endif

	if (act_len < sizeof(struct ether_header) + sizeof(struct iphdr)) {
		return NULL;
	}

	struct iphdr *ip_packet = (struct iphdr *) (eh + 1);

	return (ip_packet->saddr == check_adr);
}

void eth_print_details(const struct ether_header *eh, size_t act_len) {
	static size_t internal_counter = 0;

	printf("Debug Packet Count: %lu\n", internal_counter++);
	printf("Eth Frame Size (bytes): %lu\n", act_len);

#ifndef IPv4_ONLY
	if (eh->ether_type == htons(0x86dd)) { // IPv6 (0x86dd)
		printf("Ether type: 0x86dd (IPv6)\nSkip.\n");
		return;
	}

	if (eh->ether_type != htons(0x0800)) { // IPv4 (0x0800)
		printf("Ether type: 0x%04x (Other)\nSkip.\n", ntohs(eh->ether_type));
		return;
	}

	printf("Ether Type: 0x0800 (IPv4)\n");
#endif

	struct iphdr *ip_packet = (struct iphdr *) (eh + 1);
	printf("IPv4 Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));

	struct sockaddr_in socket_address = {0};
	socket_address.sin_addr.s_addr    = ip_packet->saddr;
	printf("Source Address: %s\n", (char *) inet_ntoa(socket_address.sin_addr));

	socket_address.sin_addr.s_addr = ip_packet->daddr;
	printf("Destination Address: %s\n", (char *) inet_ntoa(socket_address.sin_addr));

	printf("Protocol: 0x%02x", ip_packet->protocol);
	switch (ip_packet->protocol) {
		case 0x01: // ICMP
			printf(" (ICMP)\n");
			break;
		case 0x06: // TCP
			printf(" (TCP)\n");
			break;
		case 0x11: // UDP
			printf(" (UDP)\n");
			break;
		default:
			printf(" (other)\n");
	}

	printf("Identification: %d\n", ntohs(ip_packet->id));
}

void eth_print_mac(const struct ether_header *eh) {
	printf("Src MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2],
		   eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5]);
	printf("Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
		   eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);
}
