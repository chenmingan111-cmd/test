#include "Server.h"
#include <iostream>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

Server::Server(int port) 
    : port(port)
    , server_fd(-1)
    , epoll_fd(-1) 
{
    init_socket();
    init_epoll();
}

Server::~Server() {
    if (server_fd != -1) close(server_fd);
    if (epoll_fd != -1) close(epoll_fd);
}

void Server::init_socket() {
    // 1. 创建 socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "Socket created successfully!" << std::endl;

    // 2. 设置端口复用
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT failed");
        exit(EXIT_FAILURE);
    }

    // 3. 绑定
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "Bind successful!" << std::endl;

    // 4. 监听
    if (listen(server_fd, SOMAXCONN) == -1) { // 使用 SOMAXCONN 代替简单的 3
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "Listen successful!" << std::endl;
}

void Server::init_epoll() {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }
}

void Server::start() {
    std::cout << "Server started monitoring with Epoll on port " << port << "..." << std::endl;

    while (true) {
        int event_count = epoll_wait(epoll_fd, events, 10, -1);
        if (event_count == -1) {
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_fd) {
                handle_connection();
            } else {
                handle_client_data(events[i].data.fd);
            }
        }
    }
}

void Server::handle_connection() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    
    if (client_fd == -1) {
        perror("Accept failed");
        return;
    }

    // 设置新连接为非阻塞 (推荐)
    // int flags = fcntl(client_fd, F_GETFL, 0);
    // fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = client_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
        perror("Failed to add client to epoll");
        close(client_fd);
    } else {
        std::cout << "New client connected! FD: " << client_fd << std::endl;
    }
}

void Server::handle_client_data(int client_fd) {
    char buffer[1024];
    int valread = read(client_fd, buffer, sizeof(buffer) - 1);

    if (valread > 0) {
        buffer[valread] = '\0';
        client_buffers[client_fd] += buffer;

        std::string& current_buf = client_buffers[client_fd];
        size_t pos = 0;

        while ((pos = current_buf.find('\n')) != std::string::npos) {
            std::string msg = current_buf.substr(0, pos);
            std::cout << "Client[" << client_fd << "]: " << msg << std::endl;
            current_buf.erase(0, pos + 1);
        }
    } else if (valread == 0) {
        std::cout << "Client[" << client_fd << "] disconnected." << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        client_buffers.erase(client_fd);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "Client[%d] read error: %s\n", client_fd, strerror(errno));
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            close(client_fd);
            client_buffers.erase(client_fd);
        }
    }
}
