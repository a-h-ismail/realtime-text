#include "file.h"

using namespace std;

Openfile::Openfile() {}

Openfile::Openfile(const char *filename)
{
    set_file(filename);
}

void Openfile::set_file(const char *_filename)
{
    std::random_device R;
    input_file.open(_filename);
    filename.assign(_filename);
    std::string data;
    file_node tmp;

    // Read the entire file to the list
    while (true)
    {
        getline(input_file, data);
        if (input_file.fail())
            break;
        tmp.data = data;
        // Every line should have a random ID
        tmp.line_id = R();
        lines.push_back(tmp);
        // Add an id to iterator mapping
        id_to_line[tmp.line_id] = prev(lines.end());
    }
}

void Openfile::save_file()
{
    auto line = lines.begin();
    file_out.open(filename);
    while (line != lines.end())
    {
        file_out << line->data << "\n";
        ++line;
    }
    file_out.close();
}

int Openfile::insert_str_at(int32_t line_id, int32_t column, string substr)
{
    auto target_line = id_to_line[line_id];
    target_line->data.insert(column, substr);
    return 0;
}

int Openfile::remove_substr(int32_t line_id, int32_t column, int32_t count)
{
    auto target_line = id_to_line[line_id];
    // Remove line break and merge with previous line
    if (column == -1)
    {
        // We can't delete the line before the first line
        if (target_line == lines.begin())
            return 0;
        auto prev = target_line;
        --prev;
        // If the amount to delete is more than 1, we need to remove count-1 characters from the beginning
        if (count > 1)
            target_line->data.erase(0, count - 2);

        prev->data += target_line->data;
        lines.erase(target_line);
    }
    // Remove line break and merge with next line
    else if (column + count > target_line->data.size())
    {
        if (target_line == lines.end())
            return 0;

        auto next = target_line;
        ++next;
        if (count > 1)
            next->data.erase(0, count - 2);
        target_line->data += next->data;
        id_to_line.erase(next->line_id);
        lines.erase(next);
    }
    else
        target_line->data.erase(column, column + count - 1);
    return 0;
}

int Openfile::break_line_at(int32_t line_id, int32_t column, std::string substr)
{
    auto target_line = id_to_line[line_id];
    file_node tmp;
    // Generate the content of the new line below
    tmp.data = target_line->data.substr(column);
    lines.insert(next(target_line), tmp);
    // Remove the part of the line that was pushed below
    target_line->data.erase(column);
    return 0;
}

int Openfile::add_line(int32_t after_id, int32_t with_id, string data)
{
    auto target_line = id_to_line[after_id];
    file_node newline;
    newline.data = data;
    newline.line_id = with_id;
    if (target_line == lines.end())
    {
        lines.push_back(newline);
        id_to_line[with_id] = lines.end();
    }
    else
    {
        ++target_line;
        lines.insert(target_line, newline);
        --target_line;
        id_to_line[with_id] = target_line;
    }
    return 0;
}

int Openfile::remove_line(int32_t line_id)
{
    auto target_line = id_to_line[line_id];
    lines.erase(target_line);
    return 0;
}

int Openfile::replace_line(int32_t line_id, string new_data)
{
    auto target_line = id_to_line[line_id];
    target_line->data = new_data;
    return 0;
}
