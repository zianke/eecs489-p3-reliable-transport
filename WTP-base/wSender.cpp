#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <unistd.h>
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
#include "PacketHeader.h"

#define MAX_PACKET_LEN 1472

size_t assemble_packet(char *buffer, struct PacketHeader packet_header,
                       char *chunk) {
    size_t packet_header_len = sizeof(struct PacketHeader);
    size_t chunk_len = packet_header.length;
    assert(packet_header_len + chunk_len <= MAX_PACKET_LEN);

    memcpy(buffer, &packet_header, packet_header_len);
    memcpy(buffer + packet_header_len, chunk, chunk_len);

    return packet_header_len + chunk_len;
}

struct PacketHeader parse_packet_header(char *buffer) {
    struct PacketHeader packet_header;
    memcpy(&packet_header, buffer, sizeof(struct PacketHeader));
    return packet_header;
}

size_t fread_nth_chunk(char *chunk, int n, long file_len, FILE *fileptr) {
    size_t max_chunk_len = 4;
    long offset = max_chunk_len * n;
    assert(offset < file_len);

    size_t chunk_len = file_len - offset < max_chunk_len ? file_len - offset : max_chunk_len;
    long cur_offset = ftell(fileptr);
    fseek(fileptr, offset - cur_offset, SEEK_CUR);

    fread(chunk, chunk_len, 1, fileptr);
    return chunk_len;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        std::cout << "Error: Usage is ./wSender <input_file> <window_size> <log> <receiver_IP> <receiver_port>\n";
        return 1;
    }

    char *input_file = argv[1];
    int window_size = atoi(argv[2]);
    char *log = argv[3];
    char *receiver_IP = argv[4];
    int receiver_port = atoi(argv[5]);

    // Init UDP sender
    int sockfd;
    struct sockaddr_in their_addr;
    struct hostent *he;
    int numbytes;
    char buffer[MAX_PACKET_LEN];
    bzero(buffer, MAX_PACKET_LEN);

    if ((he = gethostbyname(receiver_IP)) == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(receiver_port);
    their_addr.sin_addr = *((struct in_addr *) he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);

    // Init file pointer
    FILE *fileptr;
    long file_len;
    char chunk[MAX_PACKET_LEN];
    bzero(chunk, MAX_PACKET_LEN);

    fileptr = fopen(input_file, "rb");
    fseek(fileptr, 0, SEEK_END);
    file_len = ftell(fileptr);
    rewind(fileptr);


    unsigned int chunk_len = fread_nth_chunk(chunk, 0, file_len, fileptr);

    struct PacketHeader packet_header = {2, 1, chunk_len, 123};
    size_t buffer_len = assemble_packet(buffer, packet_header, chunk);
    printf("%d", buffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        printf("[%c]", buffer[i]);
    }

    chunk_len = fread_nth_chunk(chunk, 3, file_len, fileptr);

    packet_header = {2, 1, chunk_len, 123};
    buffer_len = assemble_packet(buffer, packet_header, chunk);
    printf("%d", buffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        printf("[%c]", buffer[i]);
    }

    chunk_len = fread_nth_chunk(chunk, 2, file_len, fileptr);

    packet_header = {2, 1, chunk_len, 123};
    buffer_len = assemble_packet(buffer, packet_header, chunk);
    printf("%d", buffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        printf("[%c]", buffer[i]);
    }

    chunk_len = fread_nth_chunk(chunk, 4, file_len, fileptr);

    packet_header = {2, 1, chunk_len, 123};
    buffer_len = assemble_packet(buffer, packet_header, chunk);
    printf("%d", buffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        printf("[%c]", buffer[i]);
    }

    chunk_len = fread_nth_chunk(chunk, 1, file_len, fileptr);

    packet_header = {2, 1, chunk_len, 123};
    buffer_len = assemble_packet(buffer, packet_header, chunk);
    printf("%d", buffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        printf("[%c]", buffer[i]);
    }

//    struct PacketHeader packet_header2 = parse_packet_header(buffer);
//    printf("%d", packet_header2.type);

//    while (true) {
//        if ((numbytes = sendto(sockfd, buffer, 4, 0,
//                               (struct sockaddr *) &their_addr, sizeof(struct sockaddr))) == -1) {
//            perror("sendto");
//            exit(1);
//        }
//
//        printf("sent %d bytes to %s\n", numbytes,
//               inet_ntoa(their_addr.sin_addr));
//    }

    close(sockfd);
    fclose(fileptr);

    return 0;
}
