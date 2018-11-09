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

int main(int argc, char *argv[]) {
    if (argc < 5) {
        std::cout << "Error: Usage is ./wReceiver <port_num> <log> <window_size> <file_dir>\n";
        return 1;
    }

    int port_num = atoi(argv[1]);
    char *log = argv[2];
    int window_size = atoi(argv[3]);
    char *file_dir = argv[4];

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

    // Init file pointer
    FILE *fileptr;
    char chunk[MAX_PACKET_LEN];
    bzero(chunk, MAX_PACKET_LEN);
    fileptr = fopen(file_dir, "rb+");

    int rand_num = -1; // END seqNum should be the same as START;

    bool completed = false;
    while (!completed) {
        if ((numbytes = recvfrom(sockfd, buffer, MAX_BUFFER_LEN - 1, 0,
                                 (struct sockaddr *) &send_addr, (socklen_t *) &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        char *send_ip = inet_ntoa(send_addr.sin_addr);
        int send_port = ntohs(send_addr.sin_port);

        printf("%s, %d\n", send_ip, send_port);

        struct PacketHeader packet_header = parse_packet_header(buffer);
        bzero(chunk, MAX_PACKET_LEN);
        size_t chunk_len = parse_chunk(buffer, chunk);
        printf("%d, %d, %d, %d\n", packet_header.type, packet_header.seqNum, packet_header.length,
               packet_header.checksum);

        if (crc32(chunk, chunk_len) != packet_header.checksum) {
            printf("Checksum incorrect\n");
            continue;
        }

        if (packet_header.type == 0 && rand_num != -1) {
            printf("Duplicate START\n");
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

        size_t ACK_packet_len = assemble_packet(ACK_buffer, 3, packet_header.seqNum, 0, empty_chunk);

        if ((numbytes = sendto(sockfd, ACK_buffer, ACK_packet_len, 0,
                               (struct sockaddr *) &ACK_addr, sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
        }

        printf("sent %d bytes type %d to %s:%d\n", numbytes, 3, inet_ntoa(ACK_addr.sin_addr), ntohs(ACK_addr.sin_port));

        switch (packet_header.type) {
            case 0:
                rand_num = packet_header.seqNum;
                break;
            case 1:
                if (packet_header.seqNum == rand_num) {
                    completed = true;
                }
                break;
            case 2:
                break;
        }
    }

    close(sockfd);
    fclose(fileptr);

    return 0;
}
