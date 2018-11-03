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

    return 0;
}
