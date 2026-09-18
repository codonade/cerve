#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

int main(void) {
    // - create an IPv4 socket using a reliable, connection-oriented byte stream (TCP).
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    // - assign an address to the socket.
    struct sockaddr_in socket_address = {
        .sin_family = AF_INET,
        .sin_port = htons(3000),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK),
        },
    };
    bind(socket_fd, (struct sockaddr *)&socket_address, sizeof(socket_address));

    // - begin listening for connections.
    listen(socket_fd, 256);
    char host_name[NI_MAXHOST], service_name[NI_MAXSERV];
    getnameinfo((struct sockaddr *)&socket_address, sizeof(socket_address),
            host_name, sizeof(host_name), service_name, sizeof(service_name), 0);
    printf("Listening on http://%s:%s\n", host_name, service_name);

    return 0;
}
