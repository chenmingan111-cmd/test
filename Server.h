#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <map>
#include <sys/epoll.h>

class Server {
public:
    Server(int port);
    ~Server();
    void start();

private:
    int port;
    int server_fd;
    int epoll_fd;
    struct epoll_event events[10];
    std::map<int, std::string> client_buffers;

    void init_socket();
    void init_epoll();
    void handle_connection();
    void handle_client_data(int client_fd);
    void set_nonblocking(int fd); // Optional: Preparation for future non-blocking I/O
};

#endif
