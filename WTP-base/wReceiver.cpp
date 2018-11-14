#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "PacketHeader.h"
#include "crc32.h"

#define MAX_PACKET_LEN 1472
#define MAX_BUFFER_LEN 2048

size_t assemble_packet(char *buffer, unsigned int type, unsigned int seqNum, unsigned int length, char *chunk) {
    size_t packet_header_len = sizeof(struct PacketHeader);
    assert(packet_header_len + length <= MAX_PACKET_LEN);

    struct PacketHeader packet_header = {type, seqNum, length, crc32(chunk, length)};

    memcpy(buffer, &packet_header, packet_header_len);
    memcpy(buffer + packet_header_len, chunk, length);

    return packet_header_len + length;
}

struct PacketHeader parse_packet_header(char *buffer) {
    struct PacketHeader packet_header;
    memcpy(&packet_header, buffer, sizeof(struct PacketHeader));
    return packet_header;
}

size_t parse_chunk(char *buffer, char *chunk) {
    struct PacketHeader packet_header = parse_packet_header(buffer);
    size_t packet_len = packet_header.length;
    memcpy(chunk, buffer + sizeof(struct PacketHeader), packet_len);
    return packet_len;
}

size_t fwrite_nth_chunk(char *chunk, int n, size_t chunk_len, FILE *fileptr) {
    size_t max_chunk_len = MAX_PACKET_LEN - sizeof(struct PacketHeader);
    long offset = max_chunk_len * n;

    long cur_offset = ftell(fileptr);
    fseek(fileptr, offset - cur_offset, SEEK_CUR);

    fwrite(chunk, chunk_len, 1, fileptr);
    return chunk_len;
}

void left_shift_array(int *array, int num_elements, int shift_by) {
    assert(num_elements > 0);
    assert(shift_by > 0);
    assert(shift_by <= num_elements);

    memmove(&array[0], &array[shift_by], (num_elements - shift_by) * sizeof(int));

    for (int i = num_elements - shift_by; i < num_elements; i++) {
        array[i] = 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        std::cout << "Error: Usage is ./wReceiver <port_num> <log> <window_size> <file_dir>\n";
        return 1;
    }

    int port_num = atoi(argv[1]);
    char *log = argv[2];
    int window_size = atoi(argv[3]);
    char *file_dir = argv[4];

    // Init log file pointer
    FILE *log_fileptr = fopen(log, "a+");

    // Init UDP receiver
    int sockfd;
    struct sockaddr_in recv_addr;
    struct sockaddr_in send_addr;
    int addr_len = sizeof(struct sockaddr);
    int numbytes;
    char buffer[MAX_BUFFER_LEN];
    bzero(buffer, MAX_BUFFER_LEN);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(port_num);
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(recv_addr.sin_zero), '\0', 8);

    if (bind(sockfd, (struct sockaddr *) &recv_addr,
             sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    int file_num = 0;
    int window_start = 0;
    int status[window_size]; // 0: not received, 1: received and acked

    while (true) {
        // Init filename
        file_num++;
        FILE *fileptr = nullptr;
        char chunk[MAX_PACKET_LEN];
        bzero(chunk, MAX_PACKET_LEN);
        char output_file[strlen(file_dir) + 10];
        sprintf(output_file, "%s/FILE-%d", file_dir, file_num);

        int rand_num = -1; // END seqNum should be the same as START;

        // Reset receive window
        for (int i = 0; i < window_size; i++) {
            status[i] = 0;
        }
        window_start = 0;

        bool completed = false;
        while (!completed) {
            if ((numbytes = recvfrom(sockfd, buffer, MAX_BUFFER_LEN - 1, 0,
                                     (struct sockaddr *) &send_addr, (socklen_t *) &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }

            char *send_ip = inet_ntoa(send_addr.sin_addr);
            int send_port = ntohs(send_addr.sin_port);

            struct PacketHeader packet_header = parse_packet_header(buffer);
            fprintf(log_fileptr, "%u %u %u %u\n", packet_header.type, packet_header.seqNum, packet_header.length,
                    packet_header.checksum);
            fflush(log_fileptr);

            bzero(chunk, MAX_PACKET_LEN);
            size_t chunk_len = parse_chunk(buffer, chunk);

            if (crc32(chunk, chunk_len) != packet_header.checksum) {
                printf("Checksum incorrect\n");
                continue;
            }

            int seqNum = -1;
            bool should_continue = false;
            switch (packet_header.type) {
                case 0:
                    if (rand_num != -1 && rand_num != packet_header.seqNum) {
                        printf("Duplicate START\n");
                        should_continue = true;
                    } else {
                        rand_num = packet_header.seqNum;
                        seqNum = packet_header.seqNum;
                        if (fileptr == nullptr) {
                            fileptr = fopen(output_file, "wb+");
                            fclose(fileptr);
                            fileptr = fopen(output_file, "rb+");
                        }
                    }
                    break;
                case 1:
                    if (rand_num != packet_header.seqNum && rand_num != -1) {
                        printf("END seqNum not same as START\n");
                        should_continue = true;
                    } else {
                        seqNum = packet_header.seqNum;
                        completed = true;
                        rand_num = -1;
                    }
                    break;
                case 2:
                    if (rand_num == -1) {
                        printf("No START received\n");
                        should_continue = true;
                    } else {
                        if (packet_header.seqNum < window_start) {
                            seqNum = window_start;
                        } else if (packet_header.seqNum > window_start) {
                            seqNum = window_start;
                            if (packet_header.seqNum < window_start + window_size) {
                                if (status[packet_header.seqNum - window_start] == 0) {
                                    status[packet_header.seqNum - window_start] = 1;
                                    fwrite_nth_chunk(chunk, packet_header.seqNum, chunk_len, fileptr);
                                }
                            }
                        } else {
                            // packet_header.seqNum == window_start
                            if (status[0] == 0) {
                                status[0] = 1;
                                fwrite_nth_chunk(chunk, packet_header.seqNum, chunk_len, fileptr);
                            }

                            int shift_by = 0;
                            for (int i = 0; i < window_size; i++) {
                                if (status[i] == 1) {
                                    shift_by++;
                                } else {
                                    break;
                                }
                            }
                            window_start += shift_by;
                            seqNum = window_start;
                            left_shift_array(status, window_size, shift_by);
                        }
                    }
                    break;
                default:
                    should_continue = true;
                    break;
            }

            if (should_continue) {
                continue;
            }

            // Init ACK sender
            struct sockaddr_in ACK_addr;
            struct hostent *he;

            if ((he = gethostbyname(send_ip)) == NULL) {
                perror("gethostbyname");
                exit(1);
            }

            ACK_addr.sin_family = AF_INET;
            ACK_addr.sin_port = htons(send_port);
            ACK_addr.sin_addr = *((struct in_addr *) he->h_addr);
            memset(&(ACK_addr.sin_zero), '\0', 8);

            char ACK_buffer[MAX_BUFFER_LEN];
            bzero(ACK_buffer, MAX_BUFFER_LEN);

            char empty_chunk[1];
            bzero(ACK_buffer, 1);

            assert(seqNum >= 0);
            size_t ACK_packet_len = assemble_packet(ACK_buffer, 3, seqNum, 0, empty_chunk);

            if ((numbytes = sendto(sockfd, ACK_buffer, ACK_packet_len, 0,
                                   (struct sockaddr *) &ACK_addr, sizeof(struct sockaddr))) == -1) {
                perror("sendto");
                exit(1);
            }

            struct PacketHeader ack_packet_header = parse_packet_header(ACK_buffer);
            fprintf(log_fileptr, "%u %u %u %u\n", ack_packet_header.type, ack_packet_header.seqNum,
                    ack_packet_header.length,
                    ack_packet_header.checksum);
            fflush(log_fileptr);
        }

        fclose(fileptr);
    }

    close(sockfd);
    fclose(log_fileptr);

    return 0;
}
