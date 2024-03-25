#include <list>
#include <random>
#include <fstream>
#include <string>
#include <unordered_map>

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

public:
    std::ifstream fs;
    std::list<file_node> lines;

    Openfile();

    Openfile(const char *filename);

    void set_file(const char *filename);

    int insert_str_at(int32_t line_id, int32_t column, std::string substr);

    int break_line_at(int32_t line_id, int32_t column, std::string substr);

    int add_line(int32_t line_id);

    int remove_line(int32_t line_id);
};