#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define HOST     "0.0.0.0"
#define PORT     8000
#define BACKLOG  5
#define LOG_PATH "/tmp/log.txt"

#define LOG(fmt, ...)   printf(fmt ".\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) do { printf("ERROR: " fmt ": %s.\n", ##__VA_ARGS__, strerror(errno)); goto CLEANUP; } while (0)

void handle_connection(FILE* fp, int conn)
{
    int  size       = -1;
    char buff[1024] = {0};
    if (recv(conn, &size, sizeof(size), 0) != sizeof(size))
        ERROR("failed to receive size");

    if (recv(conn, buff, size, 0) != size)
        ERROR("failed to receive data");

    fprintf(fp, "%s\n", buff);
    fflush (fp);
    LOG("Message logged successfully");

CLEANUP:
    
    return;
}

void serve(FILE* fp)
{
    int    opt  = +1;
    int    serv = -1;
    int    conn = -1;
    int    conn_addr_size = 0;
    struct sockaddr_in serv_addr = {0};
    struct sockaddr_in conn_addr = {0};

    serv = socket(AF_INET, SOCK_STREAM, 0);
    if (serv == -1)
        ERROR("failed to create socket");

    setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(HOST);

    if (bind(serv, (struct sockaddr*) &serv_addr, sizeof(serv_addr)))
        ERROR("failed to bind to %s:%d", HOST, PORT);

    if (listen(serv, BACKLOG) != 0)
        ERROR("failed to listen");

    LOG("Listening on %s:%d", HOST, PORT);

    while (1) {

        conn = accept(serv, (struct sockaddr*) &conn_addr, &conn_addr_size);
        if (conn == -1)
            ERROR("failed to accept connection");
        LOG("Connection accepted");

        handle_connection(fp, conn);
    }

CLEANUP:

    if (conn != -1)
        close(conn);

    if (serv != -1)
        close(serv);
}

int main(int argc, char* argv[])
{
    FILE* fp = NULL;

    fp = fopen(LOG_PATH, "w");
    if (fp == NULL)
        ERROR("failed to open file %s", LOG_PATH);
    LOG("Opened %s", LOG_PATH);

    serve(fp);

CLEANUP: 

    if (fp != NULL)
        fclose(fp);

    return 0;
}

// This function is here so standard library functions are available in the PLT for shellcodes.
void dummy()
{
    char* arg[] = {NULL};
    socket(0, 0, 0);
    connect(0, NULL, 0);
    fork();
    dup2(0, 0);
    execv("", arg);
    perror("");
    exit(0);
}
