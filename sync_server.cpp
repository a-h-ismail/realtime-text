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

#define ITERATIONS_PER_SEC 50
#define ITERATION_WAIT_USEC (1000000 / ITERATIONS_PER_SEC)

// Locks the main server loop
mutex server_lock;

bool termination_requested;
bool has_unsaved_data = false;

void report_termination(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        cout << "Server is shutting down. Saving changes..." << endl;
        termination_requested = true;
    }
}

void broadcast_message(vector<Client *> &clients, payload *p)
{
    for (int i = 0; i < clients.size(); ++i)
        if (!clients[i]->closed && p->user_id != clients[i]->id)
            clients[i]->send_packet(p);
}

void process_commands(Openfile &current_file, payload *p)
{
    string data;
    switch (p->function)
    {
    case APPEND_LINE:
        break;

    case ADD_LINE:
    {
        int32_t after_id, with_id;
        READ_BIN(after_id, p->data)
        READ_BIN(with_id, p->data + 4)
        data.assign(p->data + 8, p->data_size - 8);
        current_file.add_line(after_id, with_id, data);
    }
    break;

    case REPLACE_LINE:
    {
        int32_t target_id;
        READ_BIN(target_id, p->data)
        data.assign(p->data + 4, p->data_size - 4);
        current_file.replace_line(target_id, data);
    }
    break;

    case BREAK_LINE:
    {
        int32_t target_id, column, newline_id;
        string prefix;
        READ_BIN(target_id, p->data);
        READ_BIN(column, p->data + 4);
        READ_BIN(newline_id, p->data + 8);
        prefix.assign(p->data + 12, p->data_size - 12);
        current_file.break_line_at(target_id, column, newline_id, prefix);
    }
    break;

    case ADD_STR:
    {
        int32_t line_id, column;
        READ_BIN(line_id, p->data)
        READ_BIN(column, p->data + 4)
        data.assign(p->data + 8, p->data_size - 8);
        current_file.insert_str_at(line_id, column, data);
    }
    break;

    case REMOVE_STR:
    {
        int32_t target_id, column, count;
        READ_BIN(target_id, p->data)
        READ_BIN(column, p->data + 4)
        READ_BIN(count, p->data + 8)
        current_file.remove_substr(target_id, column, count);
    }
    break;

    default:
        return;
    }
    has_unsaved_data = true;
}

void server_loop(vector<Client *> &clients, Openfile &current_file)
{
    vector<payload> commands;
    int save_timer = 0;

    while (1)
    {
        server_lock.lock();
        // Save changes to disk once every ~30 seconds
        if (save_timer == 30 * ITERATIONS_PER_SEC)
        {
            if (has_unsaved_data)
            {
                current_file.save_file();
                has_unsaved_data = false;
            }
            save_timer = 0;
        }
        else
            ++save_timer;

        // Received a termination or interrupt, cleanly exit
        if (termination_requested)
        {
            current_file.save_file();
            for (int i = 0; i < clients.size(); ++i)
                delete clients[i];
            exit(0);
        }

        // Remove disconnected clients and collect all commands
        for (int i = 0; i < clients.size(); ++i)
        {
            if (clients[i]->closed)
            {
                payload p;
                p.function = REMOVE_USER;
                p.data_size = 0;
                p.user_id = clients[i]->id;
                p.data = NULL;

                cout << "Connection To " << inet_ntoa(clients[i]->socket.sin_addr) << ":"
                     << ntohs(clients[i]->socket.sin_port) << " is closed" << endl;
                // Delete the allocated pointer then remove the pointer from the vector
                delete clients[i];
                clients.erase(clients.begin() + i);

                broadcast_message(clients, &p);
            }
            else
            {
                // Lock reception in the target client
                clients[i]->lock_recv.lock();
                // Retrieve received payloads
                commands.insert(commands.end(), clients[i]->recv_commands.begin(), clients[i]->recv_commands.end());
                // Clear the current list of commands from the client (to make room for new ones)
                clients[i]->recv_commands.clear();
                clients[i]->lock_recv.unlock();
            }
        }

        // Execute all commands on the server
        for (int i = 0; i < commands.size(); ++i)
            process_commands(current_file, &commands[i]);

        // Relay commands to other clients
        for (int i = 0; i < clients.size(); ++i)
            clients[i]->send_commands(commands);

        // Delete all payloads data then clear the vector
        for (int i = 0; i < commands.size(); ++i)
            delete[] commands[i].data;
        commands.clear();
        server_lock.unlock();
        usleep(ITERATION_WAIT_USEC);
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
    p.data = NULL;
    p.data_size = 0;
    p.function = ADD_USER;

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
        // Inform the client about all other users
        for (int i = 0; i < clients.size(); ++i)
        {
            p.user_id = clients[i]->id;
            new_arrival->send_packet(&p);
        }
        // Inform all other clients of the new client
        p.user_id = next_user_id;
        broadcast_message(clients, &p);
        // Send file content to the new client
        new_arrival->push_file(the_file);
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