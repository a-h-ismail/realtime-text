#include "client.h"

using namespace std;

Client::Client()
{
}

Client::Client(sockaddr_in s, int cl_descriptor)
{
    socket = s;
    closed = false;
    descriptor = cl_descriptor;
    send_buffer = new char[UINT16_MAX];
    recv_buffer = new char[UINT16_MAX];
}

Client::~Client()
{
    shutdown(descriptor, SHUT_RDWR);
    close(descriptor);
    instance->join();
    delete instance;
    delete[] send_buffer;
    delete[] recv_buffer;
}

void Client::start_sync()
{
    instance = new thread(client_receiver, ref(*this));
}

void Client::push_file(Openfile &file)
{
    payload p;
    auto line = file.lines.begin();
    p.function = APPEND_LINE;
    p.user_id = id;

    do
    {
        p.data_size = line->data.size() + 5;
        p.data = new char[p.data_size];
        WRITE_BIN(line->line_id, p.data);
        strcpy(p.data + 4, line->data.c_str());
        if (send_packet(&p) == -1)
        {
            closed = true;
            return;
        }
        delete[] p.data;
        ++line;
    } while (line != file.lines.end());

    // Tell the client that the initial upload is done
    p.function = END_APPEND;
    p.data_size = 0;
    p.data = NULL;
    send_packet(&p);
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
    p->function = (rt_command)recv_buffer[1];
    if (p->data_size > 0)
    {
        p->data = new char[size - 2];
        memcpy(p->data, recv_buffer + 2, size - 2);
    }
    else
        p->data = NULL;
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

void client_receiver(Client &c)
{
    payload p;
    while (c.retrieve_packet(&p) == 0)
    {
        c.lock_recv.lock();
        // Cursor move sent by the client are better handled here
        if (p.function == MOVE_CURSOR)
        {
            int32_t line, column;
            READ_BIN(line, p.data)
            READ_BIN(column, p.data + 4)
            c.cursor_line = line;
            c.cursor_x = column;
            free(p.data);
        }
        else
            c.recv_commands.push_back(p);

        c.lock_recv.unlock();
    }
    c.closed = true;
}

// Same as read(), but doesn't return unless n bytes are read (or an error occured)
int read_n(int fd, void *b, size_t n)
{
    int last, total;
    last = total = 0;
    while (total < n)
    {
        last = read(fd, (char *)b + total, n);
        if (last < 1)
            return last;
        else
            total += last;
    }
    return total;
}