#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"

#define BUFF_SIZE 1024
/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
int is_File(char *path){
	struct stat path_stat;
	stat(path,&path_stat);
	return S_ISREG(path_stat.st_mode);
}

int is_Directory(char *path){
	struct stat path_stat;
	stat(path,&path_stat);
	return S_ISDIR(path_stat.st_mode);
}

void write_from_file_to_fd(int fd,char *file_path){
	int file_des = open(file_path,O_RDONLY);
	if(file_des == -1){
			printf("fail to open file\n");
	}
  char buffer[1024];
	while(read(file_des,buffer,sizeof(buffer)) != 0){
		http_send_string(fd,buffer);
	}
	close(file_des);
}

void internal_error_res(int fd) {
    http_start_response(fd, 500);
    http_send_header(fd, "Content-type", "text/html");
    http_send_header(fd, "Server", "httpserver/1.0");
    http_end_headers(fd);
    http_send_string(fd, "<html><body>Internal Error</body></html>");
    close(fd);
}
void not_found_res(int fd) {
    http_start_response(fd, 404);
    http_send_header(fd, "Content-type", "text/html");
    http_send_header(fd, "Server", "httpserver/1.0");
    http_end_headers(fd);
    http_send_string(fd, "<html><body>File Not Found</body></html>");
    close(fd);
}
void response_file(int fd, char *file_path) {
    char file_buff[BUFF_SIZE];
    FILE *f  = fopen(file_path, "r");

    if (f == NULL) 
        return not_found_res(fd);
    http_start_response(fd, 200);
    http_send_header(fd, "Content-type",
            http_get_mime_type(file_path));
    http_send_header(fd, "Server", "httpserver/1.0");
    http_end_headers(fd);
    while(!feof(f)) {
        fgets(file_buff, BUFF_SIZE, f);
        // printf("%s", file_buff);
        http_send_string(fd, file_buff);
    }
    close(fd);
}
void response_string(int fd, char *res) {
    http_start_response(fd, 200);
    http_send_header(fd, "Content-type", "text/html");
    http_send_header(fd, "Server", "httpserver/1.0");
    http_end_headers(fd);
    http_send_string(fd, res);
    close(fd);
}
void handle_files_request(int fd)
{
    struct http_request *request = http_request_parse(fd);
    char file_path[BUFF_SIZE];
    char dir_path[BUFF_SIZE];
    char res_buffer[BUFF_SIZE];
    DIR *dp;
    struct dirent *ep;
    struct hostent* server;

    if (request == NULL) {
        return internal_error_res(fd);
    }
    strcpy(file_path, server_files_directory);
    strcat(file_path, request->path);
    printf("File path: %s\n", file_path);
    if (is_File(file_path)) {
        return response_file(fd, file_path);
    } else if (is_Directory(file_path)) {
        strcpy(dir_path, file_path);
        strcat(file_path, "index.html");
        // index.html not exist
        if (fopen(file_path, "r") == NULL) {
            dp = opendir(dir_path);
            strcpy(res_buffer, "<html><body><ul>");
            while(ep = readdir(dp)) {
                char link_buffer[BUFF_SIZE];

                strcpy(file_path, request->path);
                strcat(file_path, ep->d_name);
                sprintf(link_buffer,
                        "<li><a href=\"%s\">%s</a></li>",
                        file_path, ep->d_name);
                strcat(res_buffer, link_buffer);
            }
            strcat(res_buffer, "</ul></body></html>");
            return response_string(fd, res_buffer);
        }
        return response_file(fd, file_path);
    }
    return not_found_res(fd);
    printf("Current directory path: %s\n", server_files_directory);
    printf("HTTP method: %s\n", request->method);
    printf("Path: %s\n", request->path);
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd)
{
    struct sockaddr_in dest;
    int socketfd;
    char buff[BUFF_SIZE];
    struct addrinfo *ai;
    struct addrinfo hints;
    char port_string[BUFF_SIZE];
    struct hostent* server;
    char* msg;
    char* hostp;
    char new_hostp[BUFF_SIZE];
    char* next_headerp;
    char buffcpy[BUFF_SIZE * 2];
    char* buffcpy_start;
    int i;
    char* request_data;

    memset(&dest, 0, sizeof(dest));
    server = gethostbyname(server_proxy_hostname);
    if (server == NULL)
        perror("can not get hostname\n");
    dest.sin_port = htons(server_proxy_port);
    dest.sin_family = AF_INET;
    memcpy(&dest.sin_addr, server->h_addr, server->h_length);
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0) perror("socket() error\n");
    int socket_option = 1;
    if (connect(socketfd, (struct sockaddr*) &dest, sizeof(struct sockaddr_in)) < 0)
        perror("connect() error\n");
    while(1) {
        memset(buff, 0, sizeof(buff));
        int bread = read(fd, buff, sizeof(buff));
        if (bread == 0)
            break;
        char* send_buff = &buff[0];
        if ((hostp = strstr(buff, "Host:")) != NULL) {
            for(i = 0 ; (buff + i) != hostp ; i++) {
                *(buffcpy + i) = *(buff + i);
            }
            *(buffcpy + i) = '\0';
            sprintf(new_hostp, "Host: %s:%d\r\n", server_proxy_hostname, server_proxy_port);
            strcat(buffcpy, new_hostp);
            next_headerp = strstr(hostp, "\n") + 1;
            strcat(buffcpy, next_headerp);
            send_buff = buffcpy;
        }
        printf("%s", send_buff);
        write(socketfd, send_buff, bread);
    }
    while (1) {
        memset(&buff, 0, sizeof(buff));
        int bytes_read = read(socketfd, buff, sizeof(buff));
        if (bytes_read == 0)
            break;
        write(fd, buff, bytes_read);
    }
    return ;
    while(read(fd, buff, sizeof(buff)) > 0) {
    }
    printf("Sent\n");
    while(read(socketfd, buff, sizeof(buff)) > 0) {
        printf("%s", buff);
        write(fd, buff, sizeof(buff));
    }
    close(fd);
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int))
{
  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    fprintf(stderr, "Failed to set socket options: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr*) &server_address,
        sizeof(server_address)) == -1) {
    fprintf(stderr, "Failed to bind on socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    fprintf(stderr, "Failed to listen on socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    client_socket_number = accept(*socket_number, (struct sockaddr*) &client_address,
        (socklen_t*) &client_address_length);
    if (client_socket_number < 0) {
      fprintf(stderr, "Error accepting socket: error %d: %s\n", errno, strerror(errno));
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      signal(SIGINT, SIG_DFL); // Un-register signal handler (only parent should have it)
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "Failed to fork child: error %d: %s\n", errno, strerror(errno));
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum)
{
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(signum);
}

char *USAGE = "Usage: ./httpserver --files www_directory/ --port 8000\n"
              "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{

  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  server_files_directory = malloc(1024);
  getcwd(server_files_directory, 1024);
  server_proxy_hostname = "inst.eecs.berkeley.edu";
  server_proxy_port = 80;

  void (*request_handler)(int) = handle_files_request;

  int i;
  for (i = 1; i < argc; i++)
  {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;

}
