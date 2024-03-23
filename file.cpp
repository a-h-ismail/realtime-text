#include "file.h"

Openfile::Openfile() {}

Openfile::Openfile(const char *filename)
{
    set_file(filename);
}

void Openfile::set_file(const char *filename)
{
    std::random_device R;
    fs.open(filename);
    std::string data;
    file_node tmp;

    // Read the entire file to the list
    for (int i = 0; fs.fail() == 0; ++i)
    {
        getline(fs, data);
        tmp.data = data;
        tmp.line_no = i + 1;
        // Every line should have a random ID
        tmp.line_id = R();
        lines.push_back(tmp);
    }
}