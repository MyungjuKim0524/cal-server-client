#include <unistd.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <stdio.h> 
#include <errno.h> 
#include <limits.h> 
#include <signal.h> 
 
#define PORT 3600 
 
struct cal_data 
{ 
    int left_num; 
    int right_num; 
    char op; 
    int result; 
    short int error; 
}; 
 
static int read_exactly (int fd, void *buf, size_t n) 
{ 
    size_t got = 0; 
    unsigned char *p = (unsigned char*) buf; 
    while (got < n) 
    { 
        ssize_t r = read(fd, (p+got), (n-got)); 
        if (r == 0) 
        { 
            return 0; 
        } 
        if (r < 0) 
        { 
            if (errno == EINTR) 
            { 
                continue; 
            } 
            perror("read"); 
            return -1; 
        } 
        got += (size_t) r; 
    } 
    return 1; 
} 
 
static int write_exactly (int fd, const void *buf, size_t n) 
{ 
    size_t sent = 0; 
    const unsigned char *p = (const unsigned char*) buf; 
    while (sent < n) 
    { 
        ssize_t r = write(fd, (p+sent), (n-sent)); 
        if (r <= 0) 
        { 
            if ((r < 0) && (errno == EINTR)) 
            { 
                continue; 
            } 
            perror("write"); 
            return -1; 
        } 
        sent += (size_t) r; 
    } 
    return 1; 
} 
 
static void print_hex (const void *buf, size_t n, const char *prefix) 
{ 
    const unsigned char *p = (const unsigned char*) buf; 
    printf("%s", prefix); 
    for (size_t i = 0; i < n; ++i) 
    { 
        printf("%02x", p[i]); 
        if (i + 1 != n) 
        { 
            printf(" "); 
        } 
    } 
    printf("\n"); 
} 
 
int main (int argc, char **argv) 
{ 
    struct sockaddr_in client_addr, sock_addr; 
    int listen_sockfd, client_sockfd; 
    socklen_t addr_len; 
    pid_t pid; 
    int client_count = 0; 
 
    signal(SIGCHLD, SIG_IGN); 
 
    if ((listen_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    { 
        perror("Error "); 
        return 1; 
    } 
    memset(&sock_addr, 0x00, sizeof(sock_addr)); 
    sock_addr.sin_family = AF_INET; 
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    sock_addr.sin_port = htons(PORT); 
 
    if (bind(listen_sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1) 
    { 
        perror("Error "); 
        close(listen_sockfd); 
        return 1; 
    } 
    if (listen(listen_sockfd, 5) == -1) 
    { 
        perror("Error "); 
        close(listen_sockfd); 
        return 1; 
    } 
 
    for (;;) 
    { 
        addr_len = sizeof(client_addr); 
        client_sockfd = accept(listen_sockfd, (struct sockaddr *)&client_addr, &addr_len); 
        if (client_sockfd == -1) 
        { 
            if (errno == EINTR) 
            { 
                continue; 
            } 
            perror("accept"); 
            continue; 
        } 
 
        int client_id = ++client_count; 
 
        char ip[64]; 
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)); 
        printf("CLIENT #%d Connect : %s (%d)\n", client_id, ip, ntohs(client_addr.sin_port)); 
 
 
        pid = fork(); 
        if (pid < 0) 
        { 
            perror("fork"); 
            close(client_sockfd); 
            continue; 
        } 
        else if (pid==0) 
        { 
            close(listen_sockfd); 
 
            struct cal_data rdata; 
            int left_num, right_num, cal_result; 
            short int cal_error; 
            int nothing = 0; 
            int session_min = INT_MAX, session_max = INT_MIN; 
 
            for (;;) 
            { 
 
                if (read_exactly(client_sockfd, &rdata, sizeof(rdata)) <= 0) 
                { 
                    break; 
                } 
                cal_result = 0; 
                cal_error  = 0; 
 
                char from_client_id[64]; 
                snprintf(from_client_id, sizeof(from_client_id), "FROM CLIENT #%d : ", client_id); 
                print_hex(&rdata, sizeof(rdata), from_client_id); 
 
                left_num  = ntohl(rdata.left_num); 
                right_num = ntohl(rdata.right_num); 
 
                if (rdata.op == '$') 
                { 
                    if (!nothing) 
                    { 
                        session_min = 0; 
                        session_max = 0; 
                    } 
                    struct cal_data out = {0}; 
                    out.left_num = htonl(session_min); 
                    out.right_num = htonl(session_max); 
                    out.op = '$'; 
                    out.result = htonl(0); 
                    out.error = htons(0); 
 
                    char to_client_id[64]; 
                    snprintf(to_client_id, sizeof(to_client_id), "TO CLIENT #%d : ", client_id); 
                    print_hex(&out, sizeof(out), to_client_id); 
 
                    if (write_exactly(client_sockfd, &out, sizeof(out)) != 1) 
                    { 
                        break; 
                    } 
 
 
                    printf("Disconnected CLIENT #%d : max=%d   min=%d\n", client_id, 
session_max, session_min); 
 
                    break; 
                } 
 
                switch (rdata.op) 
                { 
                case '+': 
                    cal_result = left_num + right_num; 
                    break; 
                case '-': 
                    cal_result = left_num - right_num; 
                    break; 
                case 'x': 
                    cal_result = left_num * right_num; 
                    break; 
                case '/': 
                    if (right_num == 0) 
                    { 
                        cal_error = 2; 
                        break; 
                    } 
                    cal_result = left_num / right_num; 
                    break; 
                default: 
                    cal_error = 1; 
                } 
 
                if (cal_error == 0) 
                { 
                    if (!nothing) 
                    { 
                        nothing = 1; 
                        session_min = session_max = cal_result; 
                    } 
                    else 
                    { 
                        if (cal_result < session_min) session_min = cal_result; 
                        if (cal_result > session_max) session_max = cal_result; 
                    } 
 
                    printf("CLIENT #%d : %d %c %d = %d\n", client_id, left_num, rdata.op, 
right_num, cal_result); 
                } 
 
 
                struct cal_data out = {0}; 
                out.left_num = htonl(session_min); 
                out.right_num = htonl(session_max); 
                out.op = rdata.op; 
                out.result    = htonl(cal_result); 
                out.error     = htons(cal_error); 
 
                char to_client_id[64]; 
                snprintf(to_client_id, sizeof(to_client_id), "TO CLIENT #%d : ", client_id); 
                print_hex(&out, sizeof(out), to_client_id); 
 
                if (write_exactly(client_sockfd, &out, sizeof(out)) != 1) 
                { 
                    break; 
                } 
            } 
 
            close(client_sockfd); 
            return 0; 
        } 
        else 
        { 
            close (client_sockfd); 
        } 
    } 
    close(listen_sockfd); 
    return 0; 
} 