// Source: https://www.binarytides.com/raw-sockets-c-code-linux/

#include <arpa/inet.h>   // inet_addr
#include <errno.h>       //For errno - the error number
#include <netinet/ip.h>  //Provides declarations for ip header
#include <netinet/tcp.h> //Provides declarations for tcp header
#include <stdio.h>       //for printf
#include <stdlib.h>      //for exit(0);
#include <string.h>      //memset
#include <sys/socket.h>  //for socket ofcourse
#include <sys/types.h>
#include <unistd.h> // sleep()

// 96 bit (12 bytes) pseudo header needed for tcp header checksum calculation
struct pseudo_header {
	u_int32_t source_address;
	u_int32_t dest_address;
	u_int8_t placeholder;
	u_int8_t protocol;
	u_int16_t tcp_length;
};

static int raw_sock = -1;

// Generic checksum calculation function
unsigned short csum(unsigned short *ptr, int nbytes) {
	register long sum;
	unsigned short oddbyte;
	register short answer;

	sum = 0;
	while (nbytes > 1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	if (nbytes == 1) {
		oddbyte                = 0;
		*((u_char *) &oddbyte) = *(u_char *) ptr;
		sum += oddbyte;
	}

	sum    = (sum >> 16) + (sum & 0xffff);
	sum    = sum + (sum >> 16);
	answer = (short) ~sum;

	return (answer);
}

void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}
void create_raw_sock_tcp() {
	raw_sock = socket(PF_INET, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_TCP);

	if (raw_sock == -1) {
		die("socket(2)");
	}

	int enable = 1;

	// IP_HDRINCL: tell the kernel that headers are included in the packet
	if (setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(int)) == -1) {
		die("setsockopt(2)");
	}
}

int main(void) {
	create_raw_sock_tcp();

	char datagram[4096], source_ip[32], *data, *pseudogram;

	memset(datagram, 0, 4096);

	// IP header
	struct iphdr *iph = (struct iphdr *) datagram;

	// TCP header
	struct tcphdr *tcph = (struct tcphdr *) (datagram + sizeof(struct ip));
	struct pseudo_header psh;

	// Data part
	data = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr);
	strcpy(data, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	// some address resolution
	struct sockaddr_in sin;
	strcpy(source_ip, "127.0.0.1");
	sin.sin_family      = AF_INET;
	sin.sin_port        = htons(12345);
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");

	// Fill in the IP Header
	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = 0;
	iph->tot_len  = sizeof(struct iphdr) + sizeof(struct tcphdr) + strlen(data);
	iph->id       = htonl(54321); // Id of this packet
	iph->frag_off = 0;
	iph->ttl      = 255;
	iph->protocol = IPPROTO_TCP;
	iph->check    = 0;                    // Set to 0 before calculating checksum
	iph->saddr    = inet_addr(source_ip); // Spoof the source ip address
	iph->daddr    = sin.sin_addr.s_addr;

	// Ip checksum
	iph->check = csum((unsigned short *) datagram, iph->tot_len);

	// TCP Header
	tcph->source  = htons(1234);
	tcph->dest    = htons(80);
	tcph->seq     = 0;
	tcph->ack_seq = 0;
	tcph->doff    = 5; // tcp header size
	tcph->fin     = 0;
	tcph->syn     = 1;
	tcph->rst     = 0;
	tcph->psh     = 0;
	tcph->ack     = 0;
	tcph->urg     = 0;
	tcph->window  = htons(5840); /* maximum allowed window size */
	tcph->check   = 0;           // leave checksum 0 now, filled later by pseudo header
	tcph->urg_ptr = 0;

	// Now the TCP checksum
	psh.source_address = inet_addr(source_ip);
	psh.dest_address   = sin.sin_addr.s_addr;
	psh.placeholder    = 0;
	psh.protocol       = IPPROTO_TCP;
	psh.tcp_length     = htons(sizeof(struct tcphdr) + strlen(data));

	int psize  = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + strlen(data);
	pseudogram = malloc(psize);

	memcpy(pseudogram, (char *) &psh, sizeof(struct pseudo_header));
	memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + strlen(data));

	tcph->check = csum((unsigned short *) pseudogram, psize);

	// loop if you want to flood :)
	while (1) {
		// Send the packet
		if (sendto(raw_sock, datagram, iph->tot_len, 0, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
			perror("sendto failed");
		}
		// Data send successfully
		else {
			printf("Packet Send. Length : %d \n", iph->tot_len);
		}
		// sleep for 1 seconds
		sleep(1);
	}

	return 0;
}
