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

// Locks the main server loop
mutex server_lock;

void broadcast_message(vector<Client *> &clients, payload *p)
{
    for (int i = 0; i < clients.size(); ++i)
        if (!clients[i]->closed && p->user_id != clients[i]->id)
            clients[i]->send_packet(p);
}

void server_loop(vector<Client *> &clients)
{
    vector<payload> commands;
    while (1)
    {
        server_lock.lock();
        // Remove disconnected clients and collect all commands
        for (int i = 0; i < clients.size(); ++i)
        {
            if (clients[i]->closed)
            {
                cout << "Connection To " << inet_ntoa(clients[i]->socket.sin_addr) << ":"
                     << (uint16_t)clients[i]->socket.sin_port << " is closed" << endl;
                delete clients[i];
                clients.erase(clients.begin() + i);
                break;
            }
            // Lock reception in the target client
            clients[i]->lock_recv.lock();
            // Retrieve received payloads
            commands.insert(commands.end(), clients[i]->recv_commands.begin(), clients[i]->recv_commands.end());
            // Clear the current list of commands from the client (to make room for new ones)
            clients[i]->recv_commands.clear();
            clients[i]->lock_recv.unlock();
        }

        for (int i = 0; i < clients.size(); ++i)
            clients[i]->send_commands(commands);

        commands.clear();
        server_lock.unlock();
        usleep(50000);
    }
}

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

    thread transmitter(server_loop, ref(clients));
    transmitter.detach();

    int8_t next_user_id = 1;
    payload p = {0, 0, ADD_USER, NULL};

    while (1)
    {
        client_descriptor = accept(server_descriptor, (SA *)&client_socket, (socklen_t *)&client_size);
        server_lock.lock();
        new_arrival = new Client(client_socket, client_descriptor);
        new_arrival->id = next_user_id;
        p.user_id = -next_user_id;
        // Inform the new client of its ID (the negative ID in the payload means that this is you)
        new_arrival->send_packet(&p);
        // Inform all other clients of the new client
        p.user_id = next_user_id;
        broadcast_message(clients, &p);
        // Send file content to the new client
        new_arrival->push_file(the_file);

        if (next_user_id == INT8_MAX)
            next_user_id = 1;
        else
            ++next_user_id;

        clients.push_back(new_arrival);
        new_arrival->start_sync();
        server_lock.unlock();
    }
    return 0;
}