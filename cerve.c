#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#define streql !strcmp
char *file_mime_time(char *file_path) {
    char *extension = strchr(file_path, '.');

    // ~ Source Files
    if (streql(extension, ".html"))
        return "text/html";
    else if (streql(extension, ".css"))
        return "text/css";
    else if (streql(extension, ".js"))
        return "text/javascript";

    // ~ Media
    else if (streql(extension, ".ico"))
        return "image/x-icon";
    else if (streql(extension, ".jpg"))
        return "image/jpeg";
    else if (streql(extension, ".png"))
        return "image/png";
    else if (streql(extension, ".gif"))
        return "image/gif";

    // HMMM: Is this the best fallback for unknown files?
    else return "text/plain";
}

// TEMP: Decide on a reasonable request/response buffer sizes! 
// TEMP: Please... handle errors... I'm begging you!
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
        // - wait for requests.
        char request[1024] = {0};
        int client_socket = accept(server_socket, 0, 0);
        recv(client_socket, request, 1024, 0);

        // - parse the incoming request.
        char method[8] = {0}, target[256] = {0};
        sscanf(request, "%s %s", method, target);
        printf("%s %s\n", method, target);

        // - handle files GET requests.
        if (streql(method, "GET")) {
            // - extract a file path from the request's target.
            char file_path[256] = {0};
            if (streql(target, "/"))
                strcpy(file_path, "index.html");
            else {
                // TEMP: We assume that all targets are prefixed by a '/'
                strcpy(file_path, target + 1);
                // When we navigate to a page in a web browser, say /dashboard, the browser doesn't
                // include the `.html` extension in the request's target.
                if (!strchr(target, '.'))
                    strcat(file_path, ".html");
            }

            // - respond with the file's contents.
            char *mime_type = file_mime_time(file_path);
            int file = open(file_path, O_RDONLY);
            char header[1024] = {0};
            sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);
            send(client_socket, header, strlen(header), 0);
            // TEMP: Make sure the pass in the correct file size!
            sendfile(client_socket, file, 0, 32 * 1024);
            close(file);
        }

        close(client_socket);
    }
    return 0;
}
