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
#include <csignal>
#include "client.h"

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
    random_device R;
    int server_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    server_socket.sin_addr.s_addr = INADDR_ANY;
    server_socket.sin_port = htons(12000);
    server_socket.sin_family = AF_INET;

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

    vector<Client *> clients;
    Client *new_arrival;
    Openfile the_file("useful_text.txt");
    int client_size = sizeof(client_socket), client_descriptor;

    thread transmitter(server_loop, ref(clients), ref(the_file));
    transmitter.detach();

    int8_t next_user_id = R();
    next_user_id = abs(next_user_id);
    if (next_user_id == 0)
        ++next_user_id;
    payload p;

    while (1)
    {
        client_descriptor = accept(server_descriptor, (SA *)&client_socket, (socklen_t *)&client_size);
        cout << "Connection establised from " << inet_ntoa(client_socket.sin_addr) << ":"
             << ntohs(client_socket.sin_port) << endl;
        server_lock.lock();
        if (clients.size() > 10)
        {
            cerr << "Max client count (10) exceeded, closing the last connection...\n";
            close(client_descriptor);
            cerr << "Connection To " << inet_ntoa(client_socket.sin_addr) << ":"
                 << ntohs(client_socket.sin_port) << " is closed" << endl;
            continue;
        }
        new_arrival = new Client(client_socket, client_descriptor);
        new_arrival->id = next_user_id;
        p.user_id = -next_user_id;
        // Inform the new client of its ID (the negative ID in the payload means that this is you)
        new_arrival->send_packet(&p);
        // Send file content to the new client
        new_arrival->push_file(the_file);
        // Inform the client about all other users
        for (int i = 0; i < clients.size(); ++i)
        {
            p.function = ADD_USER;
            p.data_size = 0;
            p.user_id = clients[i]->id;
            new_arrival->send_packet(&p);
            payload d = {8, clients[i]->id, MOVE_CURSOR};
            WRITE_BIN(clients[i]->cursor_line, d.data);
            WRITE_BIN(clients[i]->cursor_x, d.data + 4);
            new_arrival->send_packet(&d);
        }
        // Inform all other clients of the new client
        p.user_id = next_user_id;
        broadcast_message(ref(clients), &p);
        // Add the new client to the clients vector and start its sync thread
        clients.push_back(new_arrival);
        new_arrival->start_sync();
        // Print new user info
        cout << "Client ID " << (int)new_arrival->id << " added." << endl;
        server_lock.unlock();
        // Find a suitable next user id that is not in use
        bool is_unique;
        do
        {
            next_user_id = R();
            next_user_id = abs(next_user_id);
            // User ID 0 is reserved for the server
            if (next_user_id == 0)
                ++next_user_id;
            is_unique = true;
            for (int i = 0; i < clients.size(); ++i)
                if (clients[i]->id == next_user_id)
                {
                    is_unique = false;
                    break;
                }
        } while (!is_unique);
    }
    return 0;
}