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
		//If hit for a message we sent, print value or something
		//Otherwise, consult the table to sent it back reverse path
		break;
	case MSG_BYE:
		//Kill tcp connection
		break;
	case MSG_JOIN:
		if (header->length == 0) {
			//Join request, send reply
			//Also add socket to writable sockets
			char *msg = build_join_accept_message(1, MSG_JOIN, PORT_DEFAULT, JOINLEN, NODE_IP, MSG_ID);
			if (send(sendersock, msg, HLEN + JOINLEN, 0) == -1)
						                perror("send");
		}else {
			//Join Reply
			//If accept, add socket to writable sockets
		}
		break;
	}
}

int main(void)
{
	//TODO: Connection to bootstrap server

	uint32_t msg_id = 1; //TODO: Create proper function
    fd_set master;    // master file descriptor list
    fd_set master_readable;
    fd_set master_writable;
    fd_set read_fds;  // temp file descriptor list for select()
    fd_set write_fds;
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char h_buf[HLEN];    // buffer for header data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&master_readable);
    FD_ZERO(&master_writable);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT_DEFAULT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);
    FD_SET(listener, &master_readable);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master_readable; // copy it
        write_fds = master_writable
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        FD_SET(newfd, &master_readable);
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, h_buf, sizeof h_buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master);
						// remove from master set
					} else {
						// we got some data from a client
						struct P2P_h* h = (struct P2P_h *) h_buf;
						if (h->version != P_VERSION || h->ttl <= 0) {
							continue;
						}
						uint8_t type = h->msg_type;
						switch (type) {
						case MSG_PING:
							if (h->ttl == PING_TTL_HB) {
								//Answer with Pong A message
								struct P2P_h h = build_header(1, MSG_PONG,
										PORT_DEFAULT, 0, NODE_IP, msg_id);
								if (send(i, h, HLEN, 0) == -1)
									perror("send");
								printf("Sent Pong A");
							} else {
								//Answer with Pong B message
							}
							break;
						case MSG_QUERY:
							//If hit, send MSG_QHIT back, send this forward to 5 peers
							break;
						case MSG_QHIT:
							//If hit for a message we sent, print value or something
							//Otherwise, consult the table to sent it back reverse path
							break;
						case MSG_BYE:
							//Kill tcp connection
							FD_CLR(i, &master);
							FD_CLR(i, &master_readable);
							FD_CLR(i, &master_writable);
							close(i);
							break;
						case MSG_JOIN:
							if (h->length == 0) {
								//Join request, send reply
								//Also add socket to writable sockets
								char *msg = build_join_accept_message(1,
										MSG_JOIN, PORT_DEFAULT, JOINLEN,
										NODE_IP, msg_id);
								if (send(i, msg, HLEN + JOINLEN, 0)
										== -1)
									perror("send");
								FD_SET(i, &master_writable);
							} else {
								//Join Reply
								//If accept, add socket to writable sockets
								char join_msg_buf[JOINLEN];
								nbytes = recv(i, join_msg_buf, sizeof join_msg_buf, 0);
								struct P2P_join* j = (struct P2P_join*)join_msg_buf;
								if (j->status == JOIN_ACC)
									FD_SET(i, &master_writable); //We were accepted, add the socket to writable sockets
							}
							break;
						}
					}
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    return 0;
}
