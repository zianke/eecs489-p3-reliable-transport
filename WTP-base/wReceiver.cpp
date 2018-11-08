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

#define MAXBUFLEN 2048

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
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    int addr_len = sizeof(struct sockaddr);
    int numbytes;
    char buffer[MAXBUFLEN];
    bzero(buffer, MAXBUFLEN);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port_num);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);

    if (bind(sockfd, (struct sockaddr *) &my_addr,
             sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    while (true) {
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN - 1, 0,
                                 (struct sockaddr *) &their_addr, (socklen_t *) &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("got packet from %s\n", inet_ntoa(their_addr.sin_addr));
        printf("packet is %d bytes long\n", numbytes);
        printf("packet contains \"%s\"\n", buffer);
    }

    close(sockfd);

    return 0;
}
