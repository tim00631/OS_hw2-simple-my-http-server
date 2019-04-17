#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
char host[10];
char port[10];
void init_argv(int argc, char *argv[], char *query_object, char *host, char *port);
void DFS_thread(void *args);
void create_thread(char* query_object);
void _mkdir(const char *path);
void write_file(const char *path, char *content);
int main(int argc, char *argv[])
{
    //initialize
    char query_object[128];
    init_argv(argc, argv, query_object, host, port);
    create_thread(query_object);

    return 0;
}
void init_argv(int argc, char *argv[], char *query_object, char *host, char *port)
{
    if (argc == 7) {
        memset(query_object, 0, sizeof(*argv[2]));
        memset(host, 0, sizeof(*argv[4]));
        memset(port, 0, sizeof(*argv[6]));
        strcat(query_object, argv[2]);
        strcat(host, argv[4]);
        strcat(port, argv[6]);
    }
}
void create_thread(char *query_object)
{
    pthread_t DFS;
    pthread_create(&DFS, NULL, DFS_thread, query_object);
    pthread_join(DFS,NULL);
}
void DFS_thread(void* args)
{
    char receiveMessage[10000];
    char receiveMessage_backup[10000];
    char query_object[128];
    char message[256];
    strcpy(query_object, (char *)args);
    int thread_count = 0;
    int sockfd = 0;
    pthread_t thread_pool[20];
    //set up socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        printf("Fail to create a socket.");
    }

    //socket connect

    struct sockaddr_in info;
    memset(&info, 0, sizeof(info));
    info.sin_family = AF_INET;

    //localhost test
    info.sin_addr.s_addr = inet_addr(host);
    info.sin_port = htons(atoi(port));

    int err = connect(sockfd, (struct sockaddr *)&info, sizeof(info));
    if (err == -1) {
        printf("Connection error\n");
    }

    //Send a message to server
    memset(message, 0, sizeof(message));
    memset(receiveMessage, 0, sizeof(message));
    sprintf(message, "GET %s HTTP/1.x\r\nHOST: %s:%s\r\n\r\n", query_object, host, port);
    send(sockfd, message, sizeof(message), 0);
    recv(sockfd, receiveMessage, sizeof(receiveMessage), 0);
    //================cut section==============
    memset(receiveMessage_backup, 0, sizeof(receiveMessage_backup));
    strcpy(receiveMessage_backup, receiveMessage);
    //HTTP/1.x 200 OK\r\nContent-Type: text/html\r\nServer: httpserver/1.x\r\n\r\nCONTENT
    char *first_line = strtok(receiveMessage_backup, "\n"); //HTTP/1.x 200 OK\r
    char *content_type = strtok(NULL, "\r");         //Content-Type: text/html
    char *trash_0 = strtok(NULL, " ");               //\nServer:
    char *trash_1 = strtok(NULL, "\n");              //httpserver/1.x\r
    char *trash_2 = strtok(NULL, "\n");              //\r
    char *content = strtok(NULL, "\0");              //CONTENT
    char *trash_3 = strtok(content_type, " ");       //Content-Type:
    char *type = strtok(NULL, " ");                  //text/html
    char *trash_4 = strtok(first_line, " ");         //HTTP/1.x
    char *status = strtok(NULL, " ");
    if(type == NULL) {
        printf("%s", receiveMessage);
    } else if (!strcmp(status,"200") &&!strcmp(type,"directory")) {
        // directory
        printf("%s", receiveMessage);
        _mkdir(query_object);
        char *pch = NULL;
        int i = 0;
        char *str[30];
        pch = strtok(content, " ");
        while (pch != NULL) { //cut every file in the directory ,saved in str[];
            str[i] = pch;
            i++;
            pch = strtok(NULL, " ");
        }
        thread_count = i-1;
        for (int j = 0; j < thread_count; j++) {
            char tempStr[150];
            int len = strlen(query_object);
            if (query_object[len - 1] == '/') {
                strcpy(tempStr, query_object);
                strcat(tempStr, str[j]);
            } else {
                strcpy(tempStr, query_object);
                strcat(tempStr, "/");
                strcat(tempStr, str[j]);
            }
            pthread_create(&(thread_pool[j]), NULL, DFS_thread, (void *)tempStr);
            usleep(500000);
        }
    } else if (!strcmp(status,"200")) {
        //the type is file
        printf("%s", receiveMessage);
        write_file(query_object, content);
    } else {
        //error
        printf("error msg\n");
    }
    for (int i = 0; i < thread_count ; i++) {
        pthread_join(thread_pool[i], NULL);
    }
    close(sockfd);
    pthread_exit(0);
}

void _mkdir(const char *query_object)
{
    char path[200];
    memset(path, 0, sizeof(path));
    char *ptr = NULL; // ./output/secfolder/db.json ...
    strcat(path, "./output");
    strcat(path, query_object);
    for (ptr = path+2; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';
            mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            *ptr = '/';
        }

    }
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
void write_file(const char* query_object,char* content)
{
    char *path[200];
    memset(path, 0, sizeof(path));
    strcat(path, "./output");
    strcat(path, query_object);
    FILE *fp = fopen(path, "w+");
    fprintf(fp, "%s", content);
    fclose(fp);
}