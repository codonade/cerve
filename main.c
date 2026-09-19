#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#define streql !strcmp

int main(void) {
    // - create an IPv4 socket using a reliable, connection-oriented byte stream (TCP).
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    // - assign an address to the socket.
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(3000),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK),
        },
    };
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    // - begin listening for connections.
    listen(server_socket, 256);
    char host_name[NI_MAXHOST], service_name[NI_MAXSERV];
    getnameinfo((struct sockaddr *)&server_address, sizeof(server_address),
            host_name, sizeof(host_name), service_name, sizeof(service_name), 0);
    printf("Listening on http://%s:%s\n", host_name, service_name);

    while (1) {
        // - parse incoming request.
        char request[1024] = {0};
        int client_socket = accept(server_socket, 0, 0);
        recv(client_socket, request, 1024, 0);
        char method[8] = {0}, route[256] = {0};
        sscanf(request, "%s %s", method, route);

        // - handle GET requests.
        // TEMP: This assumes that all GET requests ask for an HTML page!
        // TEMP: I think that we need... error handling!
        if (streql(method, "GET")) {
            char file_path[256] = {0};
            if (streql(route, "/")) sprintf(file_path, "index.html");
            else sprintf(file_path, "%s.html", route + 1);
            int file = open(file_path, O_RDONLY);
            const char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
            send(client_socket, header, strlen(header), 0);
            sendfile(client_socket, file, 0, 256);
            close(file);
        }

        close(client_socket);
    }
    return 0;
}
