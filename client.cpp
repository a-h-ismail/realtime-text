#include "client.h"
#include <condition_variable>

using namespace std;

/*
void clients_cleanup(vector<Client *> &clients)
{
    unique_lock<mutex> lock(wait_lock);
    while (1)
    {
        // Cleanup any closed connections
        for (int i = 0; i < clients.size(); ++i)
        {
            if (clients[i]->closed)
            {
                cout << "Connection To " << inet_ntoa(clients[i]->socket.sin_addr) << ":"
                     << (uint16_t)clients[i]->socket.sin_port << " is closed" << endl;
                delete clients[i];
                clients.erase(clients.begin() + i);
            }
        }
        lock.unlock();
        loop_wait.notify_all();
        usleep(20000);
        lock.lock();
    }
}
*/

Client::Client()
{
}

Client::Client(char *cname, sockaddr_in s, int cl_descriptor)
{
    name.assign(cname);
    socket = s;
    closed = false;
    descriptor = cl_descriptor;
    send_buffer = new char[1024];
    recv_buffer = new char[1024];
}

Client::~Client()
{
    instance->join();
    delete instance;
    delete send_buffer;
    delete recv_buffer;
    close(descriptor);
}

void Client::start_sync()
{
    instance = new thread(client_instance, ref(*this));
}

void Client::push_file(ifstream &upload_file)
{
    string data;
    upload_file.clear();
    upload_file.seekg(0);
    while (!upload_file.fail())
    {
        getline(upload_file, data);
        if (send_packet(APPEND_LINE, data) == -1)
        {
            closed = true;
            return;
        }
    }
    data.clear();
    // Tell the client that the initial upload is done
    send_packet(END_APPEND, data);
}

int Client::retrieve_packet(payload &p)
{
    uint16_t size;
    if (read(descriptor, recv_buffer, 1) < 1)
        return -1;

    // Check for the frame start
    if (recv_buffer[0] != '\a')
        return -1;

    if (read_n(descriptor, recv_buffer, 2) < 1)
        return -1;

    size = *(uint16_t *)(recv_buffer);

    // Read the function and its data
    if (read_n(descriptor, recv_buffer, size) < 1)
        return -1;

    p.function = (rt_command)recv_buffer[0];
    p.data.assign(recv_buffer + 1);
    return 0;
}

int Client::send_packet(rt_command function, string data)
{
    // +2 for the '\0' and the function
    uint16_t payload_size = data.size() + 2;
    // Frame start
    send_buffer[0] = '\a';
    // Payload size is bytes 1-2
    *(uint16_t *)(send_buffer + 1) = payload_size;
    // Function
    send_buffer[3] = function;
    // Data
    memcpy(send_buffer + 4, data.c_str(), payload_size);
    // +3 for the frame start and payload size
    return write(descriptor, send_buffer, payload_size + 3);
}

void client_instance(Client &c)
{
    payload p;
    while (c.retrieve_packet(p) == 0)
    {
        sleep(10);
    }
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