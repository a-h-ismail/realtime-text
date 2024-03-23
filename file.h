#include <list>
#include <random>
#include <fstream>
#include <string>

typedef struct file_node
{
    std::string data;
    int32_t line_no;
    int32_t line_id;
} file_node;

class Openfile
{
public:
    std::ifstream fs;
    std::list<file_node> lines;

    Openfile();

    Openfile(const char *filename);

    void set_file(const char *filename);
};