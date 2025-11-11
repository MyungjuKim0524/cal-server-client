#include <unistd.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <stdio.h> 
#include <errno.h> 
 
#define PORT 3600 
#define MAXLINE 1024 
 
struct cal_data 
{ 
    int left_num; 
    int right_num; 
    char op; 
    int result; 
    short int error; 
}; 
 
static int write_exactly(int fd, const void *buf, size_t n) 
{ 
    size_t sent = 0; 
    const unsigned char *p = (const unsigned char*) buf; 
    while (sent < n) 
    { 
        ssize_t w = write(fd, (p+sent), (n-sent)); 
        if (w <= 0) 
        { 
            if ((w < 0) && (errno == EINTR)) 
            { 
                continue; 
            } 
            perror("write"); 
            return -1; 
        } 
        sent += (size_t)w; 
    } 
    return 1; 
} 
 
static int read_exactly(int fd, void *buf, size_t n) 
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
        got += (size_t)r; 
    } 
    return 1; 
} 
 
static void print_hex(const void *buf, size_t n, const char *prefix) 
{ 
    const unsigned char *p = buf; 
    printf("%s", prefix); 
    for (size_t i=0; i<n; ++i) 
    { 
        printf("%02x", p[i]); 
        if (i+1 != n) printf(" "); 
    } 
    printf("\n"); 
} 
 
static void print_hex_lower(const char *buf) 
{ 
    size_t len = strlen(buf); 
    for (size_t i = 0; i < len; ++i) 
    { 
        printf("%02x", (unsigned char)buf[i]); 
        if (i < len-1) printf(" "); 
    } 
    printf("\n"); 
} 
 
int main(int argc, char *argv[]) 
{ 
    int sockfd; 
    struct sockaddr_in addr; 
 
    if (argc != 3) 
    { 
        printf("사용법: %s <local_ip> <server_ip>\n", argv[0]); 
        return 1; 
    } 
 
    const char *local_ip  = argv[1];   //내 IP
    const char *server_ip = argv[2];   // 접속할 서버 IP
 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    { 
        perror("socket"); 
        return 1; 
    } 
 
    memset(&addr, 0, sizeof(addr)); 
    addr.sin_family = AF_INET; 
    addr.sin_port = htons(PORT); 
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        perror("inet_pton server_ip");
        close(sockfd);
        return 1;
    }
 
    struct sockaddr_in local_addr; 
    memset(&local_addr, 0, sizeof(local_addr)); 
    local_addr.sin_family = AF_INET; 
    local_addr.sin_port = htons(0); 
    if (inet_pton(AF_INET, local_ip, &local_addr.sin_addr) != 1) {
        perror("inet_pton local_ip");
        close(sockfd);
        return 1;
    }
 
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) 
    { 
        perror("bind"); 
        close(sockfd); 
        return 1; 
    } 
 
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) 
    { 
        perror("connect"); 
        close(sockfd); 
        return 1; 
    } 
 
    struct cal_data sdata, rdata; 
    int a, b; 
    char op; 
 
    char buf[MAXLINE]; 
    while (1) 
    { 
        if (!fgets(buf, MAXLINE, stdin)) 
        { 
            break; 
        } 
 
        buf[strcspn(buf, "\n")] = 0; 
 
        printf("FROM KEYBOARD : "); 
        print_hex_lower(buf); 
 
        if (sscanf(buf, "%d %d %c", &a, &b, &op) != 3) 
        { 
            printf("입력 오류\n"); 
            int ch; 
            while ((ch = getchar())!='\n' && ch != EOF); 
            continue; 
        } 
 
        sdata.left_num  = htonl(a); 
        sdata.right_num = htonl(b); 
        sdata.op        = op; 
        sdata.result    = htonl(0); 
        sdata.error     = htons(0); 
 
        print_hex(&sdata, sizeof(sdata), "TO SERVER : "); 
 
        if (write_exactly(sockfd, &sdata, sizeof(sdata)) != 1) 
        { 
            break; 
        } 
        if (read_exactly(sockfd, &rdata, sizeof(rdata)) <= 0) 
        { 
            break; 
        } 
 
        print_hex(&rdata, sizeof(rdata), "FROM SERVER : "); 
 
        if (op == '$') 
        { 
            printf("Disconnected : max=%d   min=%d\n", ntohl(rdata.right_num), 
                   ntohl(rdata.left_num)); 
            fflush(stdout); 
            break; 
        } 
 
        short err = ntohs(rdata.error); 
        if (err == 0) 
        { 
            int res = ntohl(rdata.result); 
            printf("%d %c %d = %d\n", a, op, b, res); 
        } 
    } 
 
    close(sockfd); 
    return 0; 
}