#pragma once
#ifndef ETH_PACKET_H
#define ETH_PACKET_H

#include <netinet/ether.h> // struct ether_header
#include <netinet/in.h>    // in_addr_t
#include <stdbool.h>       // bool
#include <stddef.h>        // size_t

void eth_init(const char *network_interface, const uint8_t gateway_mac[6]);
void eth_deinit();

ssize_t eth_receive_frame(struct ether_header *eh, size_t max_len);

void eth_send_frame(struct ether_header *eh, size_t act_len);

bool eth_match_src_addr(const struct ether_header *eh, size_t act_len, const in_addr_t check_adr);

void eth_print_details(const struct ether_header *eh, size_t act_len);
void eth_print_mac(const struct ether_header *eh);

#endif // ETH_PACKET_H
