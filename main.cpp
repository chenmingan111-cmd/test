#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h> // 引入 Epoll 头文件
#include <cstring>     // For memset

int main() {
    //创建socket
    int result = socket(AF_INET, SOCK_STREAM, 0);
    if (result == -1) {
        std::cout << "Socket creation failed!" << std::endl;
        return -1;
    }
    std::cout << "Socket created successfully!" << std::endl;

    // 设置端口复用 (SO_REUSEADDR) - 允许在TIME_WAIT状态下绑定端口
    int opt = 1;
    if (setsockopt(result, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cout << "setsockopt failed!" << std::endl;
        return -1;
    }

    // 设置地址复用 (SO_REUSEPORT) - 允许完全重复的绑定，常用于负载均衡
    if (setsockopt(result, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        std::cout << "setsockopt SO_REUSEPORT failed!" << std::endl;
        return -1;
    }

    //创建绑定地址 
    struct sockaddr_in address;
    int addrlen = sizeof(address);  
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    //绑定成功
    int bind_result = bind(result, (struct sockaddr *)&address, sizeof(address));
    if (bind_result == -1) {
        std::cout << "Bind failed!" << std::endl;
        return -1;
    }
    std::cout << "Bind successful!" << std::endl;

    //开始监听
    int listen_result = listen(result, 3);
    if (listen_result == -1) {
        std::cout << "Listen failed!" << std::endl;
        return -1;
    }
    std::cout << "Listen successful!" << std::endl;

    // ------------------- Epoll 初始化 -------------------
    // 创建一个 epoll 实例
    // 参数 0 在现代 Linux 中被忽略，只要大于 0 即可
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cout << "epoll_create1 failed!" << std::endl;
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLIN; // 监听读事件 (有数据可读)
    event.data.fd = result; // 监听 server socket (是否有新连接到来)

    // 将 server socket 加入 epoll 监管列表
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, result, &event) == -1) {
        std::cout << "epoll_ctl failed!" << std::endl;
        return -1;
    }

    std::cout << "Server started monitoring with Epoll..." << std::endl;

    struct epoll_event events[10]; // 用来存放 epoll 返回的就绪事件
    char buffer[1024] = {0};       // 数据缓冲区

    while(true) {
        // 等待事件发生
        // 参数: epoll实例, 接收事件的数组, 数组大小, 超时时间(-1表示永久等待)
        // 返回值: 就绪的事件数量
        int event_count = epoll_wait(epoll_fd, events, 10, -1);
        
        if (event_count == -1) {
            std::cout << "epoll_wait failed!" << std::endl;
            break;
        }

        // 处理所有就绪的事件
        for(int i = 0; i < event_count; i++) {
            
            // Case 1: 监听 socket (result) 有事件 -> 表示有新连接请求
            if(events[i].data.fd == result) {
                int client_fd = accept(result, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (client_fd == -1) {
                    std::cout << "Accept failed!" << std::endl;
                    continue;
                }
                
                // 将新连接也加入 epoll 监管
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    std::cout << "Failed to add client to epoll!" << std::endl;
                    close(client_fd);
                } else {
                    std::cout << "New client connected! FD: " << client_fd << std::endl;
                }
            } 
            // Case 2: 客户端 socket 有事件 -> 表示有数据发来或断开
            else {
                int client_fd = events[i].data.fd;
                memset(buffer, 0, sizeof(buffer)); // 清空 buffer
                
                // 留一个字节给字符串结束符
                int valread = read(client_fd, buffer, sizeof(buffer) - 1);

                if (valread > 0) {
                    buffer[valread] = '\0'; // 确保字符串结束
                    std::cout << "Client[" << client_fd << "]: " << buffer << std::endl;
                } else if (valread == 0) {
                    // 客户端断开 -> 从 epoll 移除并关闭
                    std::cout << "Client[" << client_fd << "] disconnected." << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                } else {
                    // 读取出错
                    std::cout << "Client[" << client_fd << "] read error." << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                }
            }
        }
    }
    return 0;
}   