#ifndef CLIENT_H
#define CLIENT_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <vector>
#include <mutex>
#include <string.h>
#include <string>
#include <unistd.h>
#include <inttypes.h>
#include "file.h"

typedef struct sockaddr SA;

typedef enum rt_command
{
    ADD_USER,
    REMOVE_USER,
    ADD_LINE,
    APPEND_LINE,
    END_APPEND,
    ADD_STR,
    REMOVE_STR,
    REMOVE_LINE,
    MOVE_CURSOR
} rt_command;

typedef struct payload
{
    uint16_t data_size;
    int8_t user_id;
    rt_command function;
    char *data;
} payload;

// Read the data at ptr to the variable var
#define READ_BIN(var, ptr) memcpy(&var, ptr, sizeof(var));

// Write the variable var to address ptr (even if unaligned)
#define WRITE_BIN(var, ptr) memcpy(ptr, &var, sizeof(var));

class Client
{
private:
    char *send_buffer;
    char *recv_buffer;
    std::thread *instance;

public:
    Openfile *file;
    sockaddr_in socket;
    int descriptor;
    int8_t id;
    char *name;
    std::ifstream open_file;
    std::vector<payload> recv_commands;
    std::mutex lock_recv;
    bool closed;

    Client();

    Client(const Client &c)
    {
        name = c.name;
        closed = c.closed;
        descriptor = c.descriptor;
        instance = c.instance;
        send_buffer = c.send_buffer;
        recv_buffer = c.recv_buffer;
    }

    Client(char *cname, sockaddr_in s, int cl_descriptor);

    ~Client();

    void start_sync();

    void push_file(Openfile &file);

    int retrieve_packet(payload *p);

    int send_packet(payload *p);

    int send_commands(std::vector<payload> &commands);
};

int read_n(int fd, void *b, size_t n);

void client_receiver(Client &c);
#endif