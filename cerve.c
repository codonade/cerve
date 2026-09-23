#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define streql !strcmp
// NOTE: This wasn't given much thought, but for a tiny experiment like this 8KB requests seem enough.
#define REQUEST_MAX_SIZE 8 * 1024

char *file_mime_time(char *file_path) {
    char *extension = strrchr(file_path, '.');

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

int failure(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    return -1;
}

#define fail_if_error(message) do{if(error==-1)return foreign_failure(message);}while(0)
int foreign_failure(const char *message) {
    perror(message);
    return errno;
}

int main(int argc, char **argv) {
    int error;

    // - parse command line arguments.
    char directory_path[256] = {0};
    if (argc == 1) strcpy(directory_path, ".");
    else if (argc == 2) {
        strcpy(directory_path, argv[1]);
        struct stat directory_stat;
        error = stat(directory_path, &directory_stat);
        if (error == -1 && errno == ENOENT)
            return failure("'%s' not found!\n", directory_path);
        else fail_if_error("Error checking directory");
    } else return failure("USAGE: cerve [directory]\n");

    // - check the environment.
    char not_found_path[256];
    sprintf(not_found_path, "%s/404.html", directory_path);
    int not_found_file = open(not_found_path, O_RDONLY);
    if (not_found_file == -1) {
        if (errno == ENOENT) return failure("'%s' does not exist!", not_found_path);
        else return foreign_failure("Error opening 404 file");
    }
    off_t not_found_size; {
        struct stat not_found_stat;
        stat(not_found_path, &not_found_stat);
        not_found_size = not_found_stat.st_size;
    }

    // - create an IPv4 socket using a reliable, connection-oriented byte stream (TCP).
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) return foreign_failure("Error creating socket");
    error = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    fail_if_error("Error setting socket options");

    // - assign an address to the socket.
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(3000),
        .sin_addr = {
            .s_addr = htonl(INADDR_LOOPBACK),
        },
    };
    error = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    fail_if_error("Error assigning an address to the socket");

    // - begin listening for connections.
    error = listen(server_socket, 256);
    fail_if_error("Error listening for connections");
    char host_name[NI_MAXHOST], service_name[NI_MAXSERV];
    error = getnameinfo((struct sockaddr *)&server_address, sizeof(server_address),
            host_name, sizeof(host_name), service_name, sizeof(service_name), 0);
    fail_if_error("Error getting name info");
    printf("Listening on http://%s:%s\n", host_name, service_name);

    while (1) {
        // - wait for requests.
        char request[REQUEST_MAX_SIZE] = {0};
        int client_socket = accept(server_socket, 0, 0);
        if (client_socket == -1) return foreign_failure("Error accepting connections");
        error = recv(client_socket, request, REQUEST_MAX_SIZE, 0);
        fail_if_error("Error receiving messages");

        // - parse the incoming request.
        char method[8] = {0}, target[256] = {0};
        sscanf(request, "%s %s", method, target);
        printf("%s %s\n", method, target);

        // - handle files GET requests.
        if (streql(method, "GET")) {
            // - extract a file path from the request's target.
            char file_path[256] = {0};
            strcpy(file_path, directory_path);
            strcat(file_path, "/");
            if (streql(target, "/"))
                strcat(file_path, "index.html");
            else {
                // TEMP: We assume that all targets are prefixed by a '/'
                strcat(file_path, target + 1);
                // When we navigate to a page in a web browser, say /dashboard, the browser doesn't
                // include the `.html` extension in the request's target.
                if (!strchr(target, '.'))
                    strcat(file_path, ".html");
            }

            // - try opening the corresponding file.
            char *mime_type = file_mime_time(file_path);
            int file = open(file_path, O_RDONLY);
            if (file == -1) {
                if (errno == ENOENT) {
                    const char header[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
                    error = send(client_socket, header, sizeof(header), 0);
                    fail_if_error("Error sending 404 response header");
                    // NOTE: File offset gets updated by sendfile(...) call, that's why we need to go
                    // back to the start of the file everytime we want to send it.
                    lseek(not_found_file, 0, SEEK_SET);
                    error = sendfile(client_socket, not_found_file, 0, not_found_size);
                    fail_if_error("Error sending 404 response body");
                } else return foreign_failure("Error opening file");
            }
            else {
                // - get the file size in bytes.
                struct stat file_stat;
                error = stat(file_path, &file_stat);
                fail_if_error("Error getting file size");
                off_t file_size = file_stat.st_size;

                // - respond with the file's content.
                char header[1024] = {0};
                sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);
                error = send(client_socket, header, strlen(header), 0);
                fail_if_error("Error sending response header");
                error = sendfile(client_socket, file, 0, file_size);
                fail_if_error("Error sending response body");
                close(file);
            }
        }

        close(client_socket);
    }

    return 0;
}
