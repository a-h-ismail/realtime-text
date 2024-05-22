#ifndef CLIENT_H
#define CLIENT_H
#include <arpa/inet.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

typedef struct sockaddr SA;

typedef enum rt_command
{
    ADD_USER,
    REMOVE_USER,
    ADD_LINE,
    REMOVE_LINE,
    REPLACE_LINE,
    BREAK_LINE,
    APPEND_LINE,
    END_APPEND,
    ADD_STR,
    REMOVE_STR,
    MOVE_CURSOR,
    OPEN_FILE,
    STATUS
} rt_command;

enum status_msg
{
    ACCEPTED,
    FILE_INACCESSIBLE,
    CLIENTS_EXCEEDED,
    PROTOCOL_ERROR
};

#define DATA_MAX 1024

// The 5 additional bits are: frame start (1) + data size (2) + user_id (1) + function (1)
#define PREAMBLE_SIZE 5

#define PAYLOAD_MAX (DATA_MAX + PREAMBLE_SIZE)

typedef struct payload
{
    uint16_t data_size;
    int8_t user_id;
    rt_command function;
    char data[DATA_MAX];

    bool operator<(const struct payload &a)
    {
        return function < a.function;
    }
} payload;

// Read the data at ptr to the variable var
#define READ_BIN(var, ptr) memcpy(&var, ptr, sizeof(var))

// Write the variable var to address ptr (even if unaligned)
#define WRITE_BIN(var, ptr) memcpy(ptr, &var, sizeof(var))

class Client
{
private:
    std::thread instance;

public:
    sockaddr_in socket;
    int descriptor;
    int32_t cursor_x, cursor_line;
    std::vector<payload> recv_commands;
    std::mutex lock_recv;
    int8_t id;
    bool closed;
    Client();

    Client(sockaddr_in s, int cl_descriptor);

    ~Client();

    void start_sync();

    int retrieve_packet(payload *p);

    int send_packet(payload *p);

    int send_commands(std::vector<payload> &commands);

    void send_status(int8_t status);
};

int read_n(int fd, void *b, size_t n);

void client_receiver(Client *c);

void broadcast_message(std::vector<Client *> &clients, payload *p);

void handle_client_init(sockaddr_in s, int cl_descriptor);

#endif
