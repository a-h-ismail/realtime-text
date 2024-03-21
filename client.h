#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <vector>
#include <string.h>
#include <string>
#include <unistd.h>
#include <inttypes.h>

typedef struct sockaddr SA;

typedef enum rt_command
{
    STATE,
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
    rt_command function;
    std::string data;
} payload;

class Client
{
private:
    char *send_buffer;
    char *recv_buffer;
    std::thread *instance;

public:
    sockaddr_in socket;
    int descriptor;
    std::string name;
    std::ifstream open_file;
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
    
    void push_file(std::ifstream &upload_file);

    int retrieve_packet(payload &p);

    int send_packet(rt_command function, std::string data);
};

int read_n(int fd, void *b, size_t n);

void client_instance(Client &c);
