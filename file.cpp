#include "file.h"

using namespace std;

extern bool termination_requested;

std::vector<Openfile *> files;
std::mutex filelist_wlock;

void files_cleanup()
{
    int i;
    while (1)
    {
        filelist_wlock.lock();
        // Received a termination or interrupt, cleanly exit
        if (termination_requested)
        {
            for (int i = 0; i < files.size(); ++i)
            {
                files[i]->lock.lock();
                files[i]->save_file();
            }
            exit(0);
        }
        else
        {
            for (i = 0; i < files.size(); ++i)
            {
                auto tmp = files[i];
                // Acquire the lock to block the mainloop from running
                tmp->lock.lock();
                    // This open file no longer has active clients
                    if (files[i]->clients.size() == 0)
                    {
                        files.erase(files.begin() + i);
                        // After erasing the file from the list, unlock to join the main loop that should immediatly exit
                        tmp->lock.unlock();
                        tmp->mainloop.join();
                        delete tmp;
                    }
                else
                    tmp->lock.unlock();
            }
        }
        filelist_wlock.unlock();
        sleep(1);
    }
}

void Openfile::process_commands(payload *p)
{
    string data;
    switch (p->function)
    {
    case APPEND_LINE:
        break;

    case ADD_LINE:
    {
        int32_t after_id, with_id;
        READ_BIN(after_id, p->data);
        READ_BIN(with_id, p->data + 4);
        data.assign(p->data + 8, p->data_size - 8);
        add_line(after_id, with_id, data);
    }
    break;

    case REPLACE_LINE:
    {
        int32_t target_id;
        READ_BIN(target_id, p->data);
        data.assign(p->data + 4, p->data_size - 4);
        replace_line(target_id, data);
    }
    break;

    case BREAK_LINE:
    {
        int32_t target_id, column, newline_id;
        string prefix;
        READ_BIN(target_id, p->data);
        READ_BIN(column, p->data + 4);
        READ_BIN(newline_id, p->data + 8);
        prefix.assign(p->data + 12, p->data_size - 12);
        break_line_at(target_id, column, newline_id, prefix);
    }
    break;

    case ADD_STR:
    {
        int32_t line_id, column;
        READ_BIN(line_id, p->data);
        READ_BIN(column, p->data + 4);
        data.assign(p->data + 8, p->data_size - 8);
        insert_str_at(line_id, column, data);
    }
    break;

    case REMOVE_STR:
    {
        int32_t target_id, column, count;
        READ_BIN(target_id, p->data);
        READ_BIN(column, p->data + 4);
        READ_BIN(count, p->data + 8);
        remove_substr(target_id, column, count);
    }
    break;

    default:
        return;
    }
    has_unsaved_data = true;
}

Openfile::Openfile()
{
    next_id = 1;
}

Openfile::Openfile(const char *filename)
{
    next_id = 1;
    set_file(filename);
}

Openfile::~Openfile()
{
    lock.lock();
    for (int i = 0; i < clients.size(); ++i)
        delete clients[i];
    lock.unlock();
}

void Openfile::sync_loop()
{
    vector<payload> commands;
    int save_timer = 0;

    while (1)
    {
        lock.lock();

        if (clients.size() == 0)
        {
            lock.unlock();
            return;
        }

        // Save changes to disk once every ~30 seconds
        if (save_timer == 30 * ITERATIONS_PER_SEC)
        {
            if (has_unsaved_data)
            {
                save_file();
                has_unsaved_data = false;
            }
            save_timer = 0;
        }
        else
            ++save_timer;

        // Remove disconnected clients and collect all commands
        for (int i = 0; i < clients.size(); ++i)
        {
            if (clients[i]->closed)
            {
                payload p;
                p.function = REMOVE_USER;
                p.data_size = 0;
                p.user_id = clients[i]->id;

                cout << "Connection To " << inet_ntoa(clients[i]->socket.sin_addr) << ":"
                     << ntohs(clients[i]->socket.sin_port) << " is closed" << endl;
                // Delete the allocated pointer then remove the pointer from the vector
                delete clients[i];
                clients.erase(clients.begin() + i);

                broadcast_message(ref(clients), &p);
            }
            else
            {
                // Lock reception in the target client
                clients[i]->lock_recv.lock();
                // Retrieve received payloads
                commands.insert(commands.end(), clients[i]->recv_commands.begin(), clients[i]->recv_commands.end());
                // Clear the current list of commands from the client (to make room for new ones)
                clients[i]->recv_commands.clear();
                clients[i]->lock_recv.unlock();
            }
        }

        // Execute all commands on the server
        for (int i = 0; i < commands.size(); ++i)
        {
            try
            {
                process_commands(&commands[i]);
            }
            catch (out_of_range)
            {
                // If the command references a line that no longer exists, drop it
                commands.erase(commands.begin() + i);
            }
        }

        // Relay commands to other clients
        for (int i = 0; i < clients.size(); ++i)
            clients[i]->send_commands(commands);

        commands.clear();
        lock.unlock();
        usleep(ITERATION_WAIT_USEC);
    }
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

int Openfile::insert_str_at(int32_t line_id, int32_t column, string &substr)
{
    try
    {
        auto target_line = id_to_line.at(line_id);
        if (column < target_line->data.size())
            target_line->data.insert(column, substr);
        else
            target_line->data.append(substr);
        return 0;
    }
    catch (out_of_range)
    {
        return 1;
    }
}

int Openfile::remove_substr(int32_t line_id, int32_t column, int32_t count)
{
    try
    {
        auto target_line = id_to_line.at(line_id);
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
                target_line->data.erase(0, count - 1);

            prev->data += target_line->data;
            id_to_line.erase(target_line->line_id);
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
                next->data.erase(0, count - 1);
            target_line->data += next->data;
            id_to_line.erase(next->line_id);
            lines.erase(next);
        }
        else
            target_line->data.erase(column, count);
        return 0;
    }
    catch (out_of_range)
    {
        return 1;
    }
}

int Openfile::break_line_at(int32_t line_id, int32_t column, int32_t newline_id, std::string &prefix)
{
    try
    {
        auto target_line = id_to_line.at(line_id);
        file_node newline;
        // Generate the content of the new line below
        newline.data = prefix + target_line->data.substr(column);
        newline.line_id = newline_id;
        lines.insert(next(target_line), newline);
        id_to_line[newline_id] = next(target_line);
        // Remove the part of the line that was pushed below
        target_line->data.erase(column);
        return 0;
    }
    catch (out_of_range)
    {
        return 1;
    }
}

int Openfile::add_line(int32_t after_id, int32_t with_id, string &data)
{
    try
    {
        auto target_line = id_to_line.at(after_id);
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
    catch (out_of_range)
    {
        return 1;
    }
}

int Openfile::remove_line(int32_t line_id)
{
    try
    {
        auto target_line = id_to_line.at(line_id);
        lines.erase(target_line);
        return 0;
    }
    catch (out_of_range)
    {
        return 1;
    }
}

int Openfile::replace_line(int32_t line_id, string &new_data)
{
    try
    {
        auto target_line = id_to_line.at(line_id);
        target_line->data = new_data;
        return 0;
    }
    catch (out_of_range)
    {
        return 1;
    }
}

void Openfile::regen_next_id()
{

    bool is_unique;
    do
    {
        next_id = abs(next_id + 1);
        // User ID 0 is reserved for the server
        if (next_id == 0)
            ++next_id;
        is_unique = true;
        for (int i = 0; i < clients.size(); ++i)
            if (clients[i]->id == next_id)
            {
                is_unique = false;
                break;
            }
    } while (!is_unique);
}

void Openfile::add_client(Client *new_client)
{
    payload p;
    p.function = ADD_USER;
    p.data_size = 0;
    new_client->id = next_id;
    lock.lock();
    if (clients.size() == 0)
    {
        mainloop = thread([this]
                          { this->sync_loop(); });
    }
    // Inform the client of the ID assigned to it
    // The negative value indicates that this is the client's ID, not anyone else
    p.user_id = -next_id;
    new_client->send_packet(&p);

    // Send the file content first to show cursors in their correct positions
    push_file(new_client);
    // Inform the client about all other users
    for (int i = 0; i < clients.size(); ++i)
    {
        p.user_id = clients[i]->id;
        new_client->send_packet(&p);
        payload d = {8, clients[i]->id, MOVE_CURSOR};
        WRITE_BIN(clients[i]->cursor_line, d.data);
        WRITE_BIN(clients[i]->cursor_x, d.data + 4);
        new_client->send_packet(&d);
    }
    // Inform all other clients of the new client
    p.user_id = next_id;
    broadcast_message(ref(clients), &p);
    // Add the new client to the clients vector and start its sync thread
    clients.push_back(new_client);
    // Print new user info
    cout << "Client ID " << (int)new_client->id << " requested file: " << filename << endl;
    regen_next_id();
    lock.unlock();
}

void Openfile::push_file(Client *to_client)
{
    payload p;
    auto line = lines.begin();
    p.function = APPEND_LINE;
    p.user_id = to_client->id;

    do
    {
        p.data_size = line->data.size() + 5;
        WRITE_BIN(line->line_id, p.data);
        strcpy(p.data + 4, line->data.c_str());
        if (to_client->send_packet(&p) == -1)
        {
            to_client->closed = true;
            return;
        }
        ++line;
    } while (line != lines.end());

    // Tell the client that the initial upload is done
    p.function = END_APPEND;
    p.data_size = 0;
    to_client->send_packet(&p);
}
