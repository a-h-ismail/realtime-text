#ifndef FILE_H
#define FILE_H
#include "client.h"
#include <fstream>
#include <list>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

typedef struct file_node
{
    std::string data;
    int32_t line_no;
    int32_t line_id;
} file_node;

class Openfile
{
private:
    std::unordered_map<int32_t, std::list<file_node>::iterator> id_to_line;
    std::ifstream input_file;
    std::ofstream file_out;

public:
    std::thread mainloop;
    std::string filename;
    std::list<file_node> lines;
    std::mutex lock;
    std::vector<Client *> clients;
    bool has_unsaved_data;
    int8_t next_id;

    Openfile();

    Openfile(const char *filename);

    ~Openfile();

    void process_commands(payload *p);

    void add_client(Client *new_client);

    void push_file(Client *to_client);

    void sync_loop();

    void set_file(const char *filename);

    void save_file();

    int insert_str_at(int32_t line_id, int32_t column, std::string &substr);

    int remove_substr(int32_t line_id, int32_t column, int32_t count);

    int break_line_at(int32_t line_id, int32_t column, int32_t newline_id, std::string &prefix);

    int add_line(int32_t after_id, int32_t with_id, std::string &data);

    int remove_line(int32_t line_id);

    int replace_line(int32_t line_id, std::string &new_data);

    void regen_next_id();
};

#define ITERATIONS_PER_SEC 50
#define ITERATION_WAIT_USEC (1000000 / ITERATIONS_PER_SEC)

// The vector of currently open files
extern std::vector<Openfile *> files;

// This locks the openfiles vector for writing
extern std::mutex filelist_wlock;

void files_cleanup();

#endif
