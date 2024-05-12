#include "client.h"
#include "file.h"
#include <arpa/inet.h>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <list>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <vector>

using namespace std;

// Locks the main server loop
mutex server_lock;

bool termination_requested;

void report_termination(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        cout << "Server is shutting down. Saving changes..." << endl;
        termination_requested = true;
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in client_socket, server_socket;
    int server_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    server_socket.sin_addr.s_addr = INADDR_ANY;
    server_socket.sin_port = htons(12000);
    server_socket.sin_family = AF_INET;

    if (argc == 2)
        filesystem::current_path(argv[1]);
    else
    {
        cerr << "Usage: ./sync_server base_directory\n";
        return 1;
    }

    signal(SIGINT, report_termination);
    signal(SIGTERM, report_termination);

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
    else
        cout << "Server listening on " << inet_ntoa(server_socket.sin_addr) << ":"
             << ntohs(server_socket.sin_port) << endl;

    thread cleanup(files_cleanup);
    cleanup.detach();

    int client_size = sizeof(client_socket), client_descriptor;

    while (1)
    {
        client_descriptor = accept(server_descriptor, (SA *)&client_socket, (socklen_t *)&client_size);
        cout << "Connection establised from " << inet_ntoa(client_socket.sin_addr) << ":"
             << ntohs(client_socket.sin_port) << endl;

        thread handle_new_client(handle_client_init, client_socket, client_descriptor);
        handle_new_client.detach();
    }
    return 0;
}
