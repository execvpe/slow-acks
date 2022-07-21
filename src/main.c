// CAP_NET_RAW or root is required for this program to work correctly

// Adapted: https://gist.github.com/austinmarton/1922600

#include <arpa/inet.h>  // inet_addr()
#include <errno.h>      // errno
#include <pthread.h>    // pthread_create()
#include <signal.h>     // sigaction()
#include <stdio.h>      // printf()
#include <stdlib.h>     // exit()
#include <string.h>     // strerror()
#include <sys/socket.h> // socket()
#include <sys/types.h>  // socket()
#include <time.h>       // nanosleep()
#include <unistd.h>     // write()

#include "bounded_buffer.h"
#include "eth_packet.h"

#define NANOSEC(MS) (MS * 1000000)

#define WRITE(S) write(STDERR_FILENO, (S), strlen(S))

#define ETH_FRAME_SIZE 2048 // 1514 ok
#define PACKET_BUFFER_SIZE 512

struct fwd_package {
	size_t act_len;
	struct ether_header eh[];
};

static const uint8_t gateway_mac[] = {0xcc, 0xce, 0x1e, 0x3a, 0x40, 0xe8};
static bbuf_t *packet_buffer       = NULL;

static void release_and_clean(int signum) {
	(void) signum;
	WRITE("Interrupted.\n");

	// Close raw sockets
	eth_deinit();
	WRITE("Sockets closed\n");

	// Clear memory TODO
	// memset(raw_data, 0x00, ETH_FRAME_SIZE);
	// WRITE("Memory cleared.\nExit successful.\n");
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

static void *worker(void *arg) {
	pthread_detach(pthread_self());

	while (1) {
		struct fwd_package *pak = bbuf_get(packet_buffer);
		nanosleep((struct timespec *) arg, NULL);
		eth_send_frame(pak->eh, pak->act_len);
		free(pak);
	}

	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 4) {
		fprintf(stderr, "Missing argument!\n");
		exit(EXIT_FAILURE);
	}

	signal_init();

	const struct timespec delay = {.tv_sec = 0, .tv_nsec = NANOSEC(atoi(argv[1]))};
	const in_addr_t fwd_address = inet_addr(argv[3]);
	eth_init(argv[2], gateway_mac);

	packet_buffer = bbuf_create(PACKET_BUFFER_SIZE);
	if (packet_buffer == NULL) {
		release_and_clean(SIGTERM);
	}

	pthread_t fwd_thread;
	if (pthread_create(&fwd_thread, NULL, &worker, (void *) &delay) != 0) {
		release_and_clean(SIGTERM);
	}

	while (1) {
		struct fwd_package *pak = malloc(sizeof(struct fwd_package) + ETH_FRAME_SIZE);
		if (pak == NULL) {
			release_and_clean(SIGTERM);
		}

		pak->act_len = (size_t) eth_receive_frame(pak->eh, ETH_FRAME_SIZE);

		if (!eth_match_src_addr(pak->eh, pak->act_len, fwd_address)) {
			free(pak);
			continue;
		}

		printf("\n");
		eth_print_details(pak->eh, pak->act_len);
		eth_print_mac(pak->eh);

		if (!bbuf_put_nonblock(packet_buffer, pak)) {
			// Drop ethernet frame if buffer is full
			free(pak);
		}
	}

	return EXIT_SUCCESS;
}
