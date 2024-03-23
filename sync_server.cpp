#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <vector>
#include <list>
#include <random>
#include <string.h>
#include <string>
#include <unistd.h>
#include <inttypes.h>
#include "client.h"

using namespace std;

int main(int argc, char *argv[])
{
    struct sockaddr_in client_socket, server_socket;
    int server_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    server_socket.sin_addr.s_addr = INADDR_ANY;
    server_socket.sin_port = htons(12000);
    server_socket.sin_family = AF_INET;

    if (bind(server_descriptor, (SA *)&server_socket, sizeof(server_socket)) < 0)
    {
        cerr << "Unable to bind to socket...\n";
        return 1;
    }

    if (listen(server_descriptor, 10) == -1)
    {
        cerr << "Failed to start listener, exiting...\n";
        return 2;
    }

    vector<Client *> clients;
    Client *new_arrival;
    Openfile the_file("useful_text.txt");
    int client_size = sizeof(client_socket), client_descriptor;
    uint16_t payload_size;
    rt_command function;
    while (1)
    {
        client_descriptor = accept(server_descriptor, (SA *)&client_socket, (socklen_t *)&client_size);
        new_arrival = new Client("dummy", client_socket, client_descriptor);
        clients.push_back(new_arrival);
        new_arrival->push_file(the_file);
    }
    return 0;
}