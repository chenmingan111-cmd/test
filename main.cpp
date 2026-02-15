#include "Server.h"
#include <iostream>

int main() {
    Server server(8080);
    server.start();
    return 0;
}