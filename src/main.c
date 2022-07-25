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

#define WRITE(S) write(STDERR_FILENO, (S), strlen(S))

// Use shell command "ip link" to get your interface MTU
#define ETH_FRAME_SIZE 1514 // (= MTU + 14)

struct fwd_package {
	size_t act_len;
	struct ether_header eh[];
};

static uint8_t gateway_mac[8] = {0};
static char *interface_name   = NULL;

static in_addr_t forwarding_address     = 0;
static struct timespec forwarding_delay = {0};

static bbuf_t *packet_buffer     = NULL;
static size_t packet_buffer_size = 32;

static void release_and_clean(int signum) {
	(void) signum;
	WRITE("[slow] Interrupted\n");

	// Close raw sockets
	eth_deinit();
	WRITE("[slow] Sockets closed\n");

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

		printf("\n");
		eth_print_details(pak->eh, pak->act_len);
		eth_print_mac(pak->eh);

		eth_send_frame(pak->eh, pak->act_len);
		free(pak);
	}

	return NULL;
}

void die(const char *msg) {
	if (errno) {
		fprintf(stderr, "[EXIT %d] (%s) %s\n", errno, strerror(errno), msg);
	} else {
		fprintf(stderr, "[EXIT] %s\n", msg);
	}

	exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv) {
	// name, delay, interface, fwd_ip, gateway_mac, bbuf_size
	if (argc < 5) {
		fprintf(stderr, "Missing argument!\n");
		exit(EXIT_FAILURE);
	}
	// Illegal call to exec()
	if (argc <= 0 || *argv == NULL) {
		errno = EINVAL;
		die("No filename (argv[0])");
	}
	// Just the program path
	if (argc == 1) {
		errno = EINVAL;
		die("Missing argument(s)");
	}

	for (int i = 1; i < argc && argv[i] != NULL; i++) {
		char *arg = argv[i];
		if (strncmp(arg, "--", 2) != 0) {
			errno = ENOTSUP;
			die("Every argument must match \"--<key>=<value>\"!");
		}
		if (strncmp(arg, "--mac=", 6) == 0) {
			if (sscanf(arg + 6, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", gateway_mac, gateway_mac + 1,
					   gateway_mac + 2, gateway_mac + 3, gateway_mac + 4, gateway_mac + 5)
				!= 6) {
				errno = EINVAL;
				die("Invalid MAC address!");
			}
		}
		if (strncmp(arg, "--ip=", 5) == 0) {
			forwarding_address = inet_addr(arg + 5);
			continue;
		}
		if (strncmp(arg, "--delay=", 8) == 0) {
			long raw                 = atol(arg + 8);
			forwarding_delay.tv_sec  = raw / 1000;
			forwarding_delay.tv_nsec = (raw % 1000) * 1000000L;
			continue;
		}
		if (strncmp(arg, "--ifnam=", 8) == 0) {
			interface_name = arg + 8;
			continue;
		}
		if (strncmp(arg, "--bbuf=", 7) == 0) {
			packet_buffer_size = (size_t) atol(arg + 7);
			continue;
		}
	}
	if (interface_name == NULL) {
		errno = ENOTSUP;
		die("No interface name provided!");
	}
	if (forwarding_address == 0) {
		errno = ENOTSUP;
		die("No IPv4 address provided!");
	}
	for (size_t i = 0; i < 7; i++) {
		if (i == 7) {
			errno = ENOTSUP;
			die("No MAC address provided!");
		}
		if (gateway_mac[i] != 0x00) {
			break;
		}
	}

	printf("Interface Name: %s\n", interface_name);
	printf("Gateway MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", gateway_mac[0], gateway_mac[1], gateway_mac[2],
		   gateway_mac[3], gateway_mac[4], gateway_mac[5]);
	printf("Forwarding IP: %s\n", inet_ntoa((struct in_addr) {.s_addr = forwarding_address}));
	printf("Forwarding Delay: %lu ms\n", (forwarding_delay.tv_sec * 1000) + (forwarding_delay.tv_nsec / 1000000));
	printf("Packet Buffer Size: %lu Packets/Frames\n", packet_buffer_size);
}

int main(int argc, char **argv) {
	parse_args(argc, argv);
	signal_init();
	eth_init(interface_name, gateway_mac);

	packet_buffer = bbuf_create(packet_buffer_size);
	if (packet_buffer == NULL) {
		release_and_clean(SIGTERM);
	}

	pthread_t fwd_thread;
	if (pthread_create(&fwd_thread, NULL, &worker, (void *) &forwarding_delay) != 0) {
		release_and_clean(SIGTERM);
	}

	while (1) {
		struct fwd_package *pak = malloc(sizeof(struct fwd_package) + ETH_FRAME_SIZE);
		if (pak == NULL) {
			release_and_clean(SIGTERM);
		}

		pak->act_len = (size_t) eth_receive_frame(pak->eh, ETH_FRAME_SIZE);

		if (!ipv4_match_src_addr(pak->eh, pak->act_len, forwarding_address)) {
			free(pak);
			continue;
		}

		if (!bbuf_put_nonblock(packet_buffer, pak)) {
			// Drop ethernet frame if buffer is full
			free(pak);
		}
	}

	return EXIT_SUCCESS;
}
