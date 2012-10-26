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
#include <time.h>
#include "p2p_node.h"

#define BS_PORT "12100"
#define BS_IP "130.233.43.104"
#define SELF_PORT "8601"
#define ORG_PORT 0x8601

#define STDIN 0  // file descriptor for standard input

#define PORT "12100" // the port client will be connecting to

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


struct P2P_h build_header(uint8_t ttl, uint8_t msg_type, uint16_t org_port, uint16_t length, uint32_t org_ip, uint32_t msg_id) {
	struct P2P_h header;
	header.version = P_VERSION;
	header.ttl = ttl;
	header.msg_type = msg_type;
	header.reserved = 0;
	header.org_port = htons(org_port);
	header.length = htons(length);
	header.org_ip = htonl(org_ip);
	header.msg_id = htons(msg_id);
	return header;
}

char* build_join_accept_message(uint8_t ttl, uint8_t msg_type, uint16_t org_port, uint16_t length, uint32_t org_ip, uint32_t msg_id) {
	struct P2P_h h = build_header(ttl, msg_type, org_port, length, org_ip, msg_id);
	struct P2P_join j;
	j.status = JOIN_ACC;

	unsigned char h_buffer[sizeof(h)];
	memcpy(&h_buffer, &h, sizeof(h));

	unsigned char join_buffer[sizeof(j)];
	memcpy(&join_buffer, &j, sizeof(j));

	unsigned char msg[sizeof(join_buffer) + sizeof(h_buffer)];
	strcat(msg, h_buffer);
	strcat(msg, join_buffer);
	return msg;
}

//a simple random number generator for message ID
uint32_t get_msg_id()
{
    srand(time(NULL));
    return rand() % 0xFFFF;
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
    time_t send_ping; //Timer for sending ping
    time_t now;

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char h_buf[HLEN];    // buffer for header data
    int nbytes;
    char input[100];

    char s[INET6_ADDRSTRLEN];

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
    if ((rv = getaddrinfo(NULL, SELF_PORT, &hints, &ai)) != 0) {
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
    struct addrinfo* testconnect = p;
    struct sockaddr_in *self_addr = (struct sockaddr_in*)p->ai_addr;
    char *some_addr;
    some_addr = inet_ntoa(self_addr->sin_addr);

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


    int bootfd;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(BS_IP, BS_PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		printf("Daaamn");
		return 1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((bootfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("client: socket");
			continue;
		}

		if (connect(bootfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(bootfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	FD_SET(bootfd, &master);
	FD_SET(bootfd, &master_readable);

	if (bootfd > fdmax) { // keep track of the max
		fdmax = bootfd;
	}

	//Send the join request to bootstrap server
	struct P2P_h join_h = build_header(0x01, 0x03, ORG_PORT, 0, self_addr->sin_addr.s_addr, msg_id);
	unsigned char join_buffer[sizeof(join_h)];
	memcpy(join_buffer, &join_h, sizeof(join_h));
	if (send(bootfd, join_buffer, sizeof(join_buffer), 0) == -1)
		perror("send");

	//Initialize resource data
	struct P2P_resource_data data;
	data.next = NULL;
	data.resource_id = 0x0350;
	data.resource_value = 0x03500650;
	unsigned char publish_key[] = "testkey";
	FD_SET(STDIN, &master_readable);


	//Initialize peer_list data, with 1 test peer
	struct P2P_peer_data* neighbours = NULL;
	struct P2P_peer_data* peer_list_head = neighbours;
	uint16_t peer_size = 0;

	//Initialize reverse path list
	struct P2P_reverse_path_entry* reverse_path = NULL;
	struct P2P_reverse_path_entry* r_head = NULL;

	//Initialize array for message ID:s
	uint32_t sent_queries[1024];
	int sent_queries_index = 0;

	//Initialize times
	time(&send_ping);
	time(&now);


	// main loop
    for(;;) {
        read_fds = master_readable; // copy it
        write_fds = master_writable;
        if (select(fdmax+1, &read_fds, &write_fds, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

		// run through the existing connections looking for data to read
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == STDIN) {
					printf("A key was pressed\n");
					bzero(input, sizeof(input));
					read(STDIN, input, sizeof(input));
					//Send Ping A messsage
					if (strncmp(input, "pinga", strlen("pinga")) == 0) {
						for (j = 0; j <= fdmax; j++) {
							if (FD_ISSET(j, &write_fds)) {
								struct P2P_h ping_h = build_header(0x01,
										MSG_PING, ORG_PORT, 0,
										self_addr->sin_addr.s_addr, msg_id);
								unsigned char ping_buffer[sizeof(ping_h)];
								memcpy(&ping_buffer, &ping_h, sizeof(ping_h));
								if (send(j, ping_buffer,
										sizeof(ping_buffer), 0) == -1)
									perror("send");
							}
						}
						printf("Sent Ping A message\n");
					} else if (strncmp(input, "pingb", strlen("pingb")) == 0) {
						for (j = 0; j <= fdmax; j++) {
							if (FD_ISSET(j, &write_fds)) {
								struct P2P_h ping_h = build_header(0x02,
										MSG_PING, ORG_PORT, 0,
										self_addr->sin_addr.s_addr, msg_id);
								unsigned char ping_buffer[sizeof(ping_h)];
								memcpy(ping_buffer, &ping_h, sizeof(ping_h));
								if (send(j, ping_buffer,
										sizeof(ping_buffer), 0) == -1)
									perror("send");
							}
						}
						printf("Sent Ping B message\n");
					} else if (strncmp(input, "query", strlen("query")) == 0) {
						unsigned char search_key[100];
						bzero(search_key, sizeof(search_key));
						printf("Enter search key:\n");
						scanf("%s", search_key);
						struct P2P_h query_h = build_header(0x01, 0x80,
								ORG_PORT, strlen(search_key),
								self_addr->sin_addr.s_addr, get_msg_id());
						unsigned char query_buffer[sizeof(query_h)
								+ strlen(search_key)];
						memcpy(query_buffer, &query_h, sizeof(query_h));
						memcpy(query_buffer + sizeof(query_h), search_key,
								strlen(search_key));
						if (send(bootfd, query_buffer, sizeof(query_buffer), 0)
								== -1)
							perror("send");
						printf ("Sent query with key %s\n", search_key);
						if (sent_queries_index < 1024) {
							sent_queries[sent_queries_index] = ntohl(query_h.msg_id);
							sent_queries_index++;
						}
					} else if (strncmp(input, "bye", strlen("bye")) == 0) {
						struct P2P_h bye_h = build_header(0x01, MSG_BYE, ORG_PORT,
								0, self_addr->sin_addr.s_addr, msg_id);
						unsigned char join_buffer[sizeof(bye_h)];
						memcpy(join_buffer, &bye_h, sizeof(bye_h));
						if (send(bootfd, join_buffer, sizeof(join_buffer), 0)
								== -1)
							perror("send");
					} else if (strncmp(input, "qhit", strlen("qhit")) == 0) {
						int testsock;
						if ((testsock = socket(testconnect->ai_family, testconnect->ai_socktype, testconnect->ai_protocol))
									== -1) {
								perror("client: socket");
							}
						if (connect(testsock, testconnect->ai_addr, testconnect->ai_addrlen) == -1) {
									close(bootfd);
									perror("client: connect");
								}
					} else {
						printf("Command not recognized. Known commands are: 'pinga', 'pingb', 'query' and 'bye'\n");
					}
					continue;
				}
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
                            if (i == bootfd) {
                            	close(bootfd);
                            	printf("Bootstrap server refused connection, Exiting...\n");
                            	exit(0);
                            }
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master);
						// remove from master set
					} else {
						// we got some data from a client
						//struct P2P_h* h; = (struct P2P_h *) h_buf;
						struct P2P_h head;
						memcpy(&head, h_buf, sizeof(h_buf));
						struct P2P_h* h = &head;
						if (h->version != P_VERSION || h->ttl <= 0) {
							continue;
						}
						uint8_t type = h->msg_type;
						struct sockaddr_in antelope;
						uint32_t o_ip = ntohs(h->org_ip);
						memcpy(&antelope.sin_addr.s_addr, &o_ip, sizeof(uint32_t));
						char *some_addr;
						some_addr = inet_ntoa(antelope.sin_addr);
						switch (type) {
						case MSG_PING:
							if (h->ttl == PING_TTL_HB) {
								printf("Received Ping A message\n");
								//Answer with Pong A message
								h->msg_type = MSG_PONG;
								unsigned char h_buffer[HLEN];
								memcpy(h_buffer, h, HLEN);
								if (send(i, h_buffer, sizeof(h_buffer), 0) == -1)
									perror("send");
								printf("Sent Pong A\n");
							} else {
								printf("Received Ping B message.\n");
								h->msg_type = MSG_PONG;
								h->length = htons(PONG_MINLEN + peer_size * PONG_ENTRYLEN);
								struct P2P_pong_front f;
								f.entry_size = htons(peer_size);
								f.sbz = 0;
								unsigned char pongB_buffer[HLEN + PONG_MINLEN + peer_size*PONG_ENTRYLEN];
								memcpy(pongB_buffer, h, HLEN);
								memcpy(pongB_buffer+HLEN, &f, sizeof(f));
								struct P2P_peer_data* tmp = neighbours;
								int peer_count;
								for (peer_count = 0; peer_count < peer_size; peer_count++) {
									struct P2P_pong_entry e;
									e.ip = tmp->ip;
									e.port = htons(tmp->port);
									e.sbz = 0;
									memcpy(pongB_buffer+HLEN+sizeof(f)+peer_count*sizeof(e), &e, sizeof(e));
									tmp = tmp->next;
								}
								if (send(i, pongB_buffer,
										sizeof(pongB_buffer), 0) == -1)
									perror("send");
								printf ("Sent Pong B\n");

							}
							break;
						case MSG_QUERY:
						{
							printf("Received query.\n");
							//If hit, send MSG_QHIT back, send this forward to 5 peers
							unsigned char received_key[ntohs(h->length)];
							nbytes = recv(i, received_key, sizeof received_key, 0);

							//Check if the key matches, if match, create and send qhit packet
							if (strcmp(received_key, publish_key) == 0) {
								uint16_t hits = 0;
								printf("Match found, sending QueryHit\n");
								if (data.resource_id != 0) {
									struct P2P_resource_data* tmp;
									for (tmp = &data; tmp != NULL;
											tmp = tmp->next) {
										hits++;
									}
									int size =
											sizeof(struct P2P_h)
													+ sizeof(struct P2P_qhit_front)
													+ (hits
															* sizeof(struct P2P_qhit_entry));
									unsigned char qhit_buffer[size];
									int array_size = sizeof(qhit_buffer)
											/ sizeof(unsigned char);
									struct P2P_h qh_h =
											build_header(0x01, MSG_QHIT,
													ORG_PORT,
													sizeof(struct P2P_qhit_front)
															+ (hits
																	* sizeof(struct P2P_qhit_entry)),
													self_addr->sin_addr.s_addr,
													msg_id);
									memcpy(qhit_buffer, &qh_h, sizeof(qh_h));
									struct P2P_qhit_front f;
									f.entry_size = htons(hits);
									f.sbz = 0;
									memcpy(qhit_buffer + sizeof(qh_h), &f,
											sizeof(f));
									uint16_t z = 0;
									for (z = 0; z < hits; z++) {
										tmp = &data;
										struct P2P_qhit_entry e;
										e.resource_id = htons(tmp->resource_id);
										e.resource_value = htonl(
												tmp->resource_value);
										e.sbz = 0;
										memcpy(
												qhit_buffer + sizeof(qh_h)
														+ sizeof(f)
														+ (i * sizeof(e)), &e,
												sizeof(e));
										tmp = tmp->next;
									}
									if (send(i, qhit_buffer,
											sizeof(qhit_buffer), 0) == -1)
										perror("send");
								}

							}
							//Store the reverse path
							if (reverse_path == NULL) {
								reverse_path = malloc(sizeof(struct P2P_reverse_path_entry));
								reverse_path->destination_socket = i;
								reverse_path->msg_id = ntohl(h->msg_id);
								reverse_path->next = NULL;
								r_head = reverse_path;
							} else {
								struct P2P_reverse_path_entry* tmp = malloc(sizeof(struct P2P_reverse_path_entry));
								reverse_path->destination_socket = i;
								reverse_path->msg_id = ntohl(h->msg_id);
								reverse_path->next = NULL;
								r_head->next = tmp;
								r_head = tmp;
							}
							//Forward to up to 5 peers, drop ttl by one
							unsigned char whole_query[HLEN + strlen(received_key)];
							h->ttl--;
							memcpy(whole_query, h, HLEN);
							memcpy(whole_query + HLEN, received_key, strlen(received_key));
							int peers = 0;
							int fds;
							for (fds = 0; fds <= fdmax; fds++) {
								if (FD_ISSET(fds, &write_fds)) {
									if (fds != listener && fds != i) {
										if (send(fds, whole_query,
												sizeof(whole_query), 0) == -1)
											perror("send");
										peers++;
										if (peers == 5) {
											break;
										}
									}
								}
							}
						}
							break;
						case MSG_QHIT:
						{
							printf("Received query hit!\n");
							//TODO: Check if hit to our own query
							int own_query = 0;
							for (j = 0; j<1024; j++) {
								if (ntohl(h->msg_id) == sent_queries[j]) {
									own_query = 1;
								}
							}
							if (own_query) {
								printf("Received result to our own query\n");
								printf("Finding reverse path.\n");
								unsigned char qhit_front_buf[sizeof(struct P2P_qhit_front)];
								nbytes = recv(i, qhit_front_buf,
										sizeof qhit_front_buf, 0);
								struct P2P_qhit_front f;
								memcpy(&f, qhit_front_buf,
										sizeof(qhit_front_buf));
								uint16_t e = ntohs(f.entry_size);
								printf("Received entries: %u\n", e);
								for (e = 0; e < ntohs(f.entry_size); e++) {
									unsigned char entry_buf[sizeof(struct P2P_qhit_entry)];
									struct P2P_qhit_entry ent;
									nbytes = recv(i, entry_buf,
											sizeof(entry_buf), 0);
									memcpy(&ent, entry_buf, sizeof(entry_buf));
									printf("Resource value %u, %u\n", e+1, ntohl(ent.resource_value));
								}
							} else {
								//Find reverse path, drop if it doesn't exist
								printf("Finding reverse path.\n");
								unsigned char qhit_front_buf[sizeof(struct P2P_qhit_front)];
								nbytes = recv(i, qhit_front_buf,
										sizeof qhit_front_buf, 0);
								struct P2P_qhit_front f;
								memcpy(&f, qhit_front_buf,
										sizeof(qhit_front_buf));
								uint16_t e = ntohs(f.entry_size);
								unsigned char whole_qhit[HLEN
										+ sizeof(qhit_front_buf)
										+ e * sizeof(struct P2P_qhit_entry)];
								memcpy(whole_qhit, h, HLEN);
								memcpy(whole_qhit + HLEN, &f, sizeof(f));
								for (e = 0; e < ntohs(f.entry_size); e++) {
									unsigned char entry_buf[sizeof(struct P2P_qhit_entry)];
									nbytes = recv(i, entry_buf,
											sizeof(entry_buf), 0);
									memcpy(
											whole_qhit + HLEN + sizeof(f)
													+ e * sizeof(entry_buf),
											entry_buf, sizeof(entry_buf));
								}
								struct P2P_reverse_path_entry* tmp =
										reverse_path;
								for (; tmp != NULL; tmp = tmp->next) {
									if (tmp->msg_id == ntohl(h->msg_id)) {
										//Found the reverse path
										if (send(i, whole_qhit,
												sizeof(whole_qhit), 0) == -1)
											perror("send");
									}
								}
							}
						}
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
										MSG_JOIN, ORG_PORT, JOINLEN,
										self_addr->sin_addr.s_addr, msg_id);
								if (send(i, msg, HLEN + JOINLEN, 0)
										== -1)
									perror("send");
								FD_SET(i, &master_writable);
							} else {
								//Join Reply
								//If accept, add socket to writable sockets
								char join_msg_buf[JOINLEN];
								nbytes = recv(i, join_msg_buf, sizeof join_msg_buf, 0);
								struct P2P_join j;
								memcpy(&j, join_msg_buf, sizeof(join_msg_buf));
								if (j.status == JOIN_ACC || j.status == 2) {
									FD_SET(i, &master_writable); //We were accepted, add the socket to writable sockets
									if (i == bootfd) {
										printf("Established connection with the bootstrap server\n");
										//Test ping message
										struct P2P_h ping_h = build_header(0x01,
												MSG_PING, ORG_PORT, 0,
												self_addr->sin_addr.s_addr,
												msg_id);
										unsigned char ping_buffer[sizeof(ping_h)];
										memcpy(&ping_buffer, &ping_h,
												sizeof(ping_h));
										if (send(bootfd, ping_buffer,
												sizeof(ping_buffer), 0) == -1)
											perror("send");
									}
								}
							}
							break;
						case MSG_PONG:
							if (h->length == 0) {
								printf("Received Pong A message\n");
							} else {
								printf("Received Pong B message\n");
								if (h->length != 0) {
									printf("Parsing peer data");
									uint16_t payload = ntohs(h->length);
									unsigned char pong_front_buffer[PONG_MINLEN];
									nbytes = recv(i, pong_front_buffer, sizeof PONG_MINLEN, 0);
									struct P2P_pong_front front;
									memcpy(&front, pong_front_buffer, sizeof(pong_front_buffer));
									uint16_t entries = ntohs(front.entry_size);
									printf("Entry size is: %u\n", entries);
									uint16_t z = 0;
									for (z = 0; z < entries; z++) {
										unsigned char pong_entry_buffer[PONG_ENTRYLEN];
										nbytes = recv(i, pong_entry_buffer, sizeof PONG_ENTRYLEN, 0);
										struct P2P_pong_entry entry;
										memcpy(&entry, pong_entry_buffer, sizeof(pong_entry_buffer));
										char *some_addr;
										some_addr = inet_ntoa(entry.ip);
										printf("Ip-Address: %s\n", some_addr);
										if (neighbours == NULL) {
											neighbours =
													malloc(
															sizeof(struct P2P_peer_data));
											neighbours->ip = entry.ip;
											neighbours->port = entry.port;
											neighbours->next = NULL;
											peer_list_head = neighbours;
											peer_size++;
										} else {
											struct P2P_peer_data* tmp =
													malloc(
															sizeof(struct P2P_peer_data));
											tmp->ip = entry.ip;
											tmp->port = entry.port;
											tmp->next = NULL;
											peer_list_head->next = tmp;
											peer_list_head = tmp;
											peer_size++;
										}
									}

								}

							}
							break;
						}
					}
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
		//Check for time based pinging
		time(&now);
		if (now - send_ping > 5) {
			send_ping = now;
			for (j = 0; j <= fdmax; j++) {
				if (FD_ISSET(j, &write_fds)) {
					struct P2P_h ping_h = build_header(0x01, MSG_PING, ORG_PORT,
							0, self_addr->sin_addr.s_addr, msg_id);
					unsigned char ping_buffer[sizeof(ping_h)];
					memcpy(&ping_buffer, &ping_h, sizeof(ping_h));
					if (send(j, ping_buffer, sizeof(ping_buffer), 0) == -1)
						perror("send");
				}
			}
			printf("Sent time based Ping\n");
		}
    } // END for(;;)--and you thought it would never end!

    return 0;
}

