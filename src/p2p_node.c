/*
 ============================================================================
 Name        : p2p_node.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "p2p_node.h"

struct P2P_h build_header(uint8_t ttl, uint8_t msg_type, uint16_t org_port, uint16_t length, uint32_t org_ip, uint32_t msg_id) {
	struct P2P_h *header;
	header->version = P_VERSION;
	header->ttl = ttl;
	header->msg_type = msg_type;
	header->reserved = 0;
	header->org_port = htons(org_port);
	header->length = htons(length);
	header->org_ip = htons(org_ip);
	header->msg_id = htons(msg_id);
	return &header;
}

char* build_join_accept_message(uint8_t ttl, uint8_t msg_type, uint16_t org_port, uint16_t length, uint32_t org_ip, uint32_t msg_id) {
	struct P2P_h h = build_header(ttl, msg_type, org_port, length, org_ip, msg_id);
	struct P2P_join *j;
	j->status = JOIN_ACC;
	char *msg;
	strcat(msg, (char *)h);
	strcat(msg, (char *)&j);
	return msg;
}

void handle_message(struct P2P_h *header, int sendersock) {
	if (header->version != P_VERSION || header->ttl <= 0) {
		return;
	}
	uint8_t type = header->msg_type;
	switch(type) {
	case MSG_PING:
		if (header->ttl == PING_TTL_HB) {
			//Answer with Pong A message
			struct P2P_h h = build_header(1, MSG_PONG, PORT_DEFAULT, 0, NODE_IP, MSG_ID);
			if (send(sendersock, h, HLEN, 0) == -1)
			                perror("send");
		} else {
			//Answer with Pong B message
		}
		break;
	case MSG_QUERY:
		//If hit, send MSG_QHIT back, send this forward to 5 peers
		break;
	case MSG_QHIT:
		//Print results to show data found
		break;
	case MSG_BYE:
		//Kill tcp connection
		break;
	case MSG_JOIN:
		if (header->length == 0) {
			//Join request, send reply
			char *msg = build_join_accept_message(1, MSG_JOIN, PORT_DEFAULT, JOINLEN, NODE_IP, MSG_ID);
			if (send(sendersock, msg, HLEN + JOINLEN, 0) == -1)
						                perror("send");
		}else {
			//Join Reply
		}
		break;
	}
}

int main(void) {
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
