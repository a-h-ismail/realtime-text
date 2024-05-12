#include "client.h"
#include "file.h"
#include <filesystem>

using namespace std;

Client::Client()
{
}

Client::Client(sockaddr_in s, int cl_descriptor)
{
    socket = s;
    closed = false;
    descriptor = cl_descriptor;
}

Client::~Client()
{
    shutdown(descriptor, SHUT_RDWR);
    close(descriptor);
    if (instance.joinable())
        instance.detach();
}

void Client::start_sync()
{
    payload p;
    if (retrieve_packet(&p) != 0)
    {
        bool closed = true;
        return;
    }
    // Get the requested file name
    char filename[p.data_size + 1];
    memcpy(filename, p.data, p.data_size);
    filename[p.data_size] = '\0';
    // To avoid symbolic links or relative path tricks that would end up like ../../../../etc/passwd
    // Use realpath and get a proper canonical path
    char *full_path = realpath(filename, NULL);
    string pwd = filesystem::current_path().string(), relative_path;
    // Does this file not exist or the canonical path doesn't start with the current working directory?
    // If so, don't bother with this client
    if (full_path == NULL || strncmp(pwd.c_str(), full_path, pwd.size()) != 0)
    {
        free(full_path);
        cout << "Requested file is inaccessible!" << endl;
        throw bad_exception();
    }
    else
    {
        relative_path.assign(full_path + pwd.size() + 1);
        free(full_path);
    }
    // Write lock the file list
    filelist_wlock.lock();
    // Which of the open files have the requested file open?
    bool file_is_open = false;
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i]->filename == relative_path)
        {
            file_is_open = true;
            auto target_file = files[i];
            id = target_file->next_id;
            p.user_id = -id;
            // Inform the new client of its ID (the negative ID in the payload means that this is you)
            send_packet(&p);
            target_file->add_client(this);
            break;
        }
    }
    // Create a new Openfile instance and add this client to it
    if (file_is_open == false)
    {
        Openfile *new_file_instance = new Openfile(relative_path.c_str());
        new_file_instance->add_client(this);
        files.push_back(new_file_instance);
    }
    instance = thread(client_receiver, this);
    filelist_wlock.unlock();
}

int Client::retrieve_packet(payload *p)
{
    uint16_t size;
    if (read(descriptor, recv_buffer, 1) < 1)
        return -1;

    // Check for the frame start
    if (recv_buffer[0] != '\a')
        return -1;

    if (read_n(descriptor, recv_buffer, 2) < 1)
        return -1;

    READ_BIN(size, recv_buffer);

    // Read the user_id, function and its data
    if (read_n(descriptor, recv_buffer, size) < 1)
        return -1;

    p->user_id = recv_buffer[0];
    p->data_size = size - 2;

    // If a client is not obeying by size rules, terminate its connection
    if (p->data_size > PAYLOAD_MAX)
        return -1;

    if (p->data_size > 0)
        memcpy(p->data, recv_buffer + 2, size - 2);

    p->function = (rt_command)recv_buffer[1];
    return 0;
}

int Client::send_packet(payload *p)
{
    // +2 for the function and user id
    uint16_t payload_size = p->data_size + 2;
    // Frame start
    send_buffer[0] = '\a';
    // Payload size is bytes 1-2
    WRITE_BIN(payload_size, send_buffer + 1);

    send_buffer[3] = p->user_id;
    // Function
    send_buffer[4] = p->function;
    // Data
    memcpy(send_buffer + 5, p->data, p->data_size);
    // +3 for the frame start and payload size
    return send(descriptor, send_buffer, payload_size + 3, MSG_NOSIGNAL);
}

int Client::send_commands(vector<payload> &commands)
{
    int i;
    for (i = 0; i < commands.size(); ++i)
    {
        if (commands[i].user_id != id)
            send_packet(&commands[i]);
    }
    return 0;
}

void client_receiver(Client *c)
{
    payload p;
    while (c->retrieve_packet(&p) == 0)
    {
        // Discard packets where the incoming user ID doesn't match the current client
        if (c->id == p.user_id)
        {
            c->lock_recv.lock();
            // Update the known cursor position
            if (p.function == MOVE_CURSOR)
            {
                int32_t line, column;
                READ_BIN(line, p.data);
                READ_BIN(column, p.data + 4);
                c->cursor_line = line;
                c->cursor_x = column;
            }
            c->recv_commands.push_back(p);
            c->lock_recv.unlock();
        }
    }
    c->closed = true;
}

// Same as read(), but doesn't return unless n bytes are read (or an error occured)
int read_n(int fd, void *b, size_t n)
{
    int last, total;
    total = 0;
    while (total < n)
    {
        last = read(fd, (char *)b + total, n - total);
        if (last < 1)
            return last;
        else
            total += last;
    }
    return total;
}

void broadcast_message(vector<Client *> &clients, payload *p)
{
    for (int i = 0; i < clients.size(); ++i)
        if (!clients[i]->closed && p->user_id != clients[i]->id)
            clients[i]->send_packet(p);
}

// This function wraps client initialization to allow the main thread to launch a separate thread per new client
void handle_client_init(sockaddr_in s, int cl_descriptor)
{
    Client *new_client;
    try
    {
        new_client = new Client(s, cl_descriptor);
        new_client->start_sync();
    }
    catch (bad_exception)
    {
        delete new_client;
    }
}
