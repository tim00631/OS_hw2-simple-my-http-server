#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include "status.h"
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
struct node {
    //Linked list struct node
    int data;
    struct node *next;
};
struct node *Q_HEAD = NULL;
struct node *Q_TAIL = NULL;
int Q_NUM = 0;
struct sockaddr_in serverInfo, clientInfo;
int addrlen = sizeof(clientInfo);
int sockfd = 0;
char pathname[256];
char message[10000];
char *cut_string[];
char root[50];
char inputBuffer[256];
char content[10000];
int status = 0;
char description[30];
char type[20];
const char *get_description[] = {
    "OK",
    "BAD_REQUEST",
    "NOT_FOUND",
    "METHOD_NOT_ALLOWED",
    "UNSUPPORT_MEDIA_TYPE"
};
void main_thread(void *args);
void Push(int data); //Linked list Push
int Pop(void);       //Linked list pop
int isEmpty(void);   // Linked list isEmpty
void cut_pathname(char *str);
int file_or_directory(void);
int error_check(char *query_object);
void *threadpool_thread(void *args);
void fill_in_message(char *query_object);
void get_directory(char *root); //get directory information
void init_argv(int argc, char *argv[], char *root, char *port, char *thread_number);
void memset_function();
void read_file();
int main(int argc, char *argv[])
{
    char port[50];
    char thread_number[50];
    init_argv(argc, argv, root, port, thread_number);
    //printf("root:%s\nport:%s\nthread_number:%s\n", root, port, thread_number);
    //set up socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        printf("Fail to create a socket.");
    }

    //socket connection
    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverInfo.sin_port = htons(atoi(port));
    bind(sockfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo));

    //create main thread to schedule this thread pool
    pthread_t main_t;
    pthread_create(&main_t, NULL, main_thread, NULL);

    //create thread pool
    int thread_count = atoi(thread_number);
    pthread_t *thread_pool = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&(thread_pool[i]), NULL, threadpool_thread, (void *)&i);
        usleep(5000);
    }
    pthread_join(main_t, NULL);
    return 0;
}

void main_thread(void *args)
{
    while (1) {
        listen(sockfd, 5);
        int forClientSockfd = accept(sockfd, (struct sockaddr *)&clientInfo, &addrlen);
        pthread_mutex_lock(&lock);
        Push(forClientSockfd);
        memset(&forClientSockfd, 0, sizeof(forClientSockfd));
        pthread_mutex_unlock(&lock);
    }
}

void Push(int data) //Linked list Push
{
    if (Q_HEAD == NULL) {
        Q_HEAD = (struct node *)malloc(sizeof(struct node));
        Q_HEAD->data = data;
        Q_HEAD->next = NULL;
        Q_TAIL = Q_HEAD;
    } else {
        struct node *ptr = (struct node *)malloc(sizeof(struct node));
        ptr->data = data;
        ptr->next = NULL;
        Q_TAIL->next = ptr;
        Q_TAIL = ptr;
    }
    Q_NUM++;
}

int Pop(void) //Linked list pop
{
    struct node *ptr = Q_HEAD;
    int result = ptr->data;
    Q_HEAD = ptr->next;
    free(ptr);
    Q_NUM--;
    return result;
}

int isEmpty(void) // Linked list isEmpty
{
    if (Q_NUM == 0)
        return 1;
    else
        return 0;
}
void cut_pathname(char *str)
{
    char *delim = " "; // token
    char *pch;
    int i = 0;
    pch = strtok(str, delim);
    for (i = 0; i < 4; i++) {
        cut_string[i] = pch;
        pch = strtok(NULL, delim);
    }

    memset(pathname, 0, sizeof(root) + sizeof(cut_string[1]));
    strcat(pathname, root);
    strcat(pathname, cut_string[1]);
}
void *threadpool_thread(void *args)
{
    while (1) {
        pthread_mutex_lock(&lock);
        // Critical Sec.
        if (!isEmpty()) {
            int request_id = 0;
            request_id = Pop();
            memset_function();
            recv(request_id, inputBuffer, sizeof(inputBuffer), 0); //recv request
            printf("%s\n", inputBuffer);
            cut_pathname(inputBuffer);
            fill_in_message(cut_string[1]);
            send(request_id, message, sizeof(message), 0); //sent content type
        }
        // Critical Sec.
        pthread_mutex_unlock(&lock);
    }
}

void fill_in_message(char *query_object)
{
    error_check(query_object);
    memset(message, 0, sizeof(message));
    sprintf(message, "HTTP/1.x %d %s\r\nContent-Type: %s\r\nServer: httpserver/1.x\r\n\r\n%s", status, description, type, content);
}
int file_or_directory(void)
{
    struct stat sb;
    if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        //Directory example: testdir/
        return 1;
    } else if (stat(pathname, &sb) == 0 && S_ISREG(sb.st_mode)) {
        //File example: testdir/example.html
        return 0;
    } else if(stat(pathname, &sb) != 0) {
        //Not Found! example: testdir/example.json
        return -1;
    }
}
int error_check(char *query_object)
{
    int index = 0;//choose message to return
    int Usupport = 1; //check if the extendsion is not support
    char *GET = cut_string[0]; //method name
    char *file_extendsion = strtok(query_object,".");
    file_extendsion = strtok(NULL, "."); //cut extendsion from pathname bug:directory is NULL
    if (query_object[0] != '/') {
        index = 1; //Bad Request
    } else if (strcmp(GET, "GET")) {
        index = 3; //Method Not Allowed
    } else if (file_extendsion == NULL) { //no extendsion
        if(file_or_directory() == 1) {
            index = 0;
            memset(type, 0,sizeof(type));
            strcpy(type, "directory"); //directory
            get_directory(pathname);
        } else {
            index = 2; //directory not found
            memset(type, 0, sizeof(type));
            strcpy(type, "");
        }
    } else { //Not found or File
        for (int i = 0; i < 8; i++) {
            //compare extendsion
            if (!strcmp(file_extendsion, extensions[i].ext)) {
                memset(type, 0, sizeof(type));
                strcpy(type, extensions[i].mime_type); //if support, copy to type
                Usupport = 0;
                index = 0;
            }
        }
        if (Usupport) {
            index = 4; //Unsupported Media Type
            strcpy(type, "");
        } else {
            if(file_or_directory() == -1) {
                index = 2;//Not Found
                memset(type, 0, sizeof(type));
                strcpy(type, "");
            } else if(file_or_directory() == 0) {
                index = 0; //file
                read_file(pathname);
            }
        }
    }
    status = status_code[index];
    memset(description, 0, sizeof(description));
    strcpy(description,get_description[index]);
    return 0;
}
void get_directory(char *path) //get directory information
{
    DIR *d;
    struct dirent *dir;
    d = opendir(path);

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")) {
                strcat(content, dir->d_name);
                strcat(content, " ");
            }
        }
        strcat(content, "\n");
        closedir(d);
    }
}
void init_argv(int argc, char *argv[], char *root, char *port, char *thread_number)
{
    if (argc == 7) {
        memset(root, 0, sizeof(*argv[2]));
        memset(port, 0, sizeof(*argv[4]));
        memset(thread_number, 0, sizeof(*argv[6]));
        strcat(root, argv[2]);
        strcat(port, argv[4]);
        strcat(thread_number, argv[6]);
    }
};
void memset_function()
{
    memset(inputBuffer, 0, sizeof(inputBuffer));
    memset(content, 0, sizeof(content));
    memset(description, 0, sizeof(description));
    memset(type, 0, sizeof(type));
    status = 0;
}
void read_file(char* path)  // Read file
{
    char buffer[10000];
    char ch;
    int i = 0;
    FILE * file;
    file = fopen(path, "r");
    while ((ch = fgetc(file)) != EOF) {
        buffer[i] = ch;
        i++;
    }
    fclose(file);
    if (buffer) {
        sprintf(content,"%s", buffer);
    }
}
