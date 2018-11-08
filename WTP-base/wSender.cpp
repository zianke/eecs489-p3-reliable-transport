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

    int sockfd;
    struct sockaddr_in their_addr; // connector's address information
    struct hostent *he;
    int numbytes;
    char *message = "this is a test message";

    if ((he = gethostbyname(receiver_IP)) == NULL) {  // get the host info
        perror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;     // host byte order
    their_addr.sin_port = htons(receiver_port); // short, network byte order
    their_addr.sin_addr = *((struct in_addr *) he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8); // zero the rest of the struct

    if ((numbytes = sendto(sockfd, message, strlen(message), 0,
                           (struct sockaddr *) &their_addr, sizeof(struct sockaddr))) == -1) {
        perror("sendto");
        exit(1);
    }

    printf("sent %d bytes to %s\n", numbytes,
           inet_ntoa(their_addr.sin_addr));

    close(sockfd);

    return 0;
}
