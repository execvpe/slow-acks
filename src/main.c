// CAP_NET_RAW or root is required for this program to work correctly

// Adapted: https://gist.github.com/austinmarton/1922600

#include <arpa/inet.h>  // inet_addr()
#include <errno.h>      // errno
#include <signal.h>     // sigaction()
#include <stdio.h>      // printf()
#include <stdlib.h>     // exit()
#include <string.h>     // strerror()
#include <sys/socket.h> // socket()
#include <sys/types.h>  // socket()
#include <unistd.h>     // usleep()

#include "eth_packet.h"

#define msleep(X) usleep(1000 * (X))
#define WRITE(S) write(STDERR_FILENO, (S), strlen(S))

#define ETH_FRAME_SIZE 2048 // (1518) to big // 1522 // 1514 ok

static const uint8_t gateway_mac[] = {0xcc, 0xce, 0x1e, 0x3a, 0x40, 0xe8};
static uint8_t raw_data[ETH_FRAME_SIZE];

static void release_and_clean(int signum) {
	(void) signum;
	WRITE("Interrupted.\n");

	// Close raw sockets
	eth_deinit();
	WRITE("Sockets closed\n");

	// Clear memory
	memset(raw_data, 0x00, ETH_FRAME_SIZE);
	WRITE("Memory cleared.\nExit successful.\n");
	exit(EXIT_SUCCESS);
}

static void signal_init() {
	struct sigaction ign = {
		.sa_handler = SIG_IGN,
		.sa_flags   = SA_RESTART,
	};
	sigemptyset(&ign.sa_mask);
	sigaction(SIGPIPE, &ign, NULL);

	struct sigaction term = {
		.sa_handler = &release_and_clean,
	};
	sigemptyset(&term.sa_mask);
	sigaction(SIGTERM, &term, NULL);
	sigaction(SIGINT, &term, NULL);
}

int main(int argc, char **argv) {
	if (argc < 4) {
		fprintf(stderr, "Missing argument!\n");
		exit(EXIT_FAILURE);
	}

	const int delay = atoi(argv[1]);

	signal_init();
	eth_init(argv[2], gateway_mac);

	in_addr_t valid_adr             = inet_addr(argv[3]);
	struct ether_header *frame_data = (void *) raw_data;
	size_t recv_size;

	while (1) {
		recv_size = (size_t) eth_receive_frame(frame_data, ETH_FRAME_SIZE);

		if (!eth_match_src_addr(frame_data, recv_size, valid_adr)) {
			continue;
		}

		printf("\n");
		eth_print_details(frame_data, recv_size);
		eth_print_mac(frame_data);

		eth_send_frame(frame_data, recv_size);

		eth_print_mac(frame_data);

		msleep(delay);
	}

	return EXIT_SUCCESS;
}
