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
    std::ifstream input_file;
    std::ofstream file_out;

public:
    std::string filename;
    std::list<file_node> lines;

    Openfile();

    Openfile(const char *filename);

    void set_file(const char *filename);

    void save_file();

    int insert_str_at(int32_t line_id, int32_t column, std::string &substr);

    int remove_substr(int32_t line_id, int32_t column, int32_t count);

    int break_line_at(int32_t line_id, int32_t column, int32_t newline_id, std::string &prefix);

    int add_line(int32_t after_id, int32_t with_id, std::string &data);

    int remove_line(int32_t line_id);

    int replace_line(int32_t line_id, std::string &new_data);
};
