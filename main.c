#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

const int SERVER_CAPACITY = 3;
const char* greeting = "Enter your name, please: \0";
const char* ERROR_CAPACITY = "# Unfortunately, server is full...\r\n# Try again later\r\n\0";


typedef struct User_Info{
    char* buff;
    char* buff_to_send;
    char Username[17];
    int fd;
    int initial_named;
} User_Info;

int Check_Arguments(int argc, char* arg){
    if ((argc != 2) || (atoi(arg) == 0)){
        return -1;
    } else {
        return 0;
    }
}

int Connect_Port(struct sockaddr_in* a, int port_number, int sock){
    int error_bind;
    if ((port_number < 1024) || (port_number > 65535)){
        return -1;
    }
    a->sin_port = htons(port_number);
    error_bind = bind(sock, (struct sockaddr*) a, sizeof(*a));
    if (error_bind < 0){
        return -2;
    }
    return 0;
}

int Update_Descriptors(User_Info* Clients, fd_set* descriptors, int user_count, int main_descriptor){
    int max_descriptor = main_descriptor;
    FD_ZERO((descriptors));
    FD_SET(main_descriptor, descriptors);
    for (int i = 0; i < user_count; i++){
        FD_SET(Clients[i].fd, descriptors);
        if (Clients[i].fd > max_descriptor){
            max_descriptor = Clients[i].fd;
        }
    }
    return max_descriptor;
}

int Check_Name(User_Info** Clients, char name[129], int user_count, int user_number){
    ulong length = strlen(name);
    int status_check;
    int i = 0;
    if ((length < 3) || (length > 16)) {
        return -1;
    }
    while (i < length) {
        if (((name[i] >= 'A') && (name[i] <= 'Z')) || ((name[i] >= 'a') && (name[i] <= 'z')) || (name[i] == '_')) {
            i++;
            continue;
        }
        break;
    }
    if (i != length){
        return -1;
    }
    i = 0;
    status_check = strcmp(name, (*Clients)[i].Username);
    while ((i < user_count) && ((status_check != 0) || (i == user_number))) {
        i++;
        status_check = strcmp(name, (*Clients)[i].Username);
    }
    if ((status_check == 0) && (i != user_number)) {
        return -2;
    }
    return 0;
}

void Transfer_Name(User_Info** Clients, char name[129], int user_number, int user_count){
    char message[129];
    sprintf(message, "# %s changed name to '%s'\r\n", (*Clients)[user_number].Username, name);
    for (int i = 0; i <= strlen(name); i++){
        (*Clients)[user_number].Username[i] = name[i];
    }
    printf("%s", message);
    for (int i = 0; i < user_count; i++){
        if ((i != user_number) && ((*Clients)[i].initial_named == 0)) {
            if (send((*Clients)[i].fd, message, strlen(message), 0) < 0) {
                printf("Ошибка отправки сообщения!\n");
            }
        }
    }
}

void Send_Message(User_Info** Clients, int user_number, int user_count){
    char* send_buffer;
    if (strlen((*Clients)[user_number].buff_to_send) < 3){
        free((*Clients)[user_number].buff_to_send);
        return;
    }
    send_buffer = malloc(strlen((*Clients)[user_number].buff_to_send) + 25);
    sprintf(send_buffer, "[%s]: %s", (*Clients)[user_number].Username, (*Clients)[user_number].buff_to_send);
    printf("%s", send_buffer);
    for (int i = 0; i < user_count; i++){
        if ((i != user_number) && ((*Clients)[i].initial_named != 1)) {
            if (send((*Clients)[i].fd, send_buffer, strlen(send_buffer), 0) < 0){
                printf("Ошибка отправки сообщения!\n");
            }
        }
    }

    free((*Clients)[user_number].buff_to_send);
    //(*Clients)[user_number].buff_to_send = malloc(sizeof(char));
    free(send_buffer);
}

void Delete_Client(User_Info** Clients, int* user_count, int user_number){
    char send_buffer[129];
    sprintf(send_buffer, "# %s left the chat\r\n", (*Clients)[user_number].Username);
    printf("%s", send_buffer);
    for (int i = 0; i < *user_count; i++){
        if ((i != user_number) && ((*Clients)[i].initial_named != 1)){
            if (send((*Clients)[i].fd, send_buffer, strlen(send_buffer), 0) < 0){
                printf("Ошибка отправки сообщения!\n");
            }
        }
    }
    free((*Clients)[user_number].buff);
    free((*Clients)[user_number].buff_to_send);
    shutdown((*Clients)[user_number].fd, 2);
    close((*Clients)[user_number].fd);
    (*Clients)[user_number] = (*Clients)[*user_count - 1];
    (*user_count)--;
}

void Handle_Message(User_Info** Clients, char message_buffer[129], int user_number, int* user_count){
    ulong k = strlen((*Clients)[user_number].buff);
    int name_status = 0;
    char send_buffer[129];
    (*Clients)[user_number].buff = realloc((*Clients)[user_number].buff, 129 + strlen((*Clients)[user_number].buff));
    for (int i = 0; i < strlen(message_buffer); i++){
        if (((*Clients)[user_number].buff[k] = message_buffer[i]) == '\n'){
            (*Clients)[user_number].buff_to_send = malloc(k+2);
            for (int l = 0; l <= k; l++){
                (*Clients)[user_number].buff_to_send[l] = (*Clients)[user_number].buff[l];
            }
            (*Clients)[user_number].buff_to_send[k + 1] = '\0';
            if (strcmp((*Clients)[user_number].buff_to_send, "bye!\r\n") == 0){
                Delete_Client(Clients, user_count, user_number);
                return;
            }
            if ((*Clients)[user_number].initial_named == 1) {
                (*Clients)[user_number].buff_to_send[k - 1] = '\0';
                name_status = Check_Name(Clients, (*Clients)[user_number].buff_to_send, *user_count, user_number);
                if (name_status == 0) {
                    Transfer_Name(Clients, (*Clients)[user_number].buff_to_send, user_number, *user_count);
                    sprintf(send_buffer, "Welcome, %s!\r\n", (*Clients)[user_number].Username);
                    (*Clients)[user_number].initial_named = 0;
                } else if (name_status == -1){
                    sprintf(send_buffer, "# Invalid name\r\n%s", greeting);
                } else {
                    sprintf(send_buffer, "# Such name already exists\r\n%s", greeting);
                }
                if (send((*Clients)[user_number].fd, send_buffer, strlen(send_buffer), 0) < 0){
                    printf("Ошибка отправки сообщения!\n");
                }
                free((*Clients)[user_number].buff_to_send);
                //(*Clients)[user_number].buff_to_send = malloc(sizeof(char));
                free((*Clients)[user_number].buff);
                (*Clients)[user_number].buff = malloc(129 * sizeof(char));
                k = 0;
            } else {
                k = 0;
                free((*Clients)[user_number].buff);
                (*Clients)[user_number].buff = malloc(129 * sizeof(char));
                Send_Message(Clients, user_number, *user_count);
            }
        }
        k++;
    }
    (*Clients)[user_number].buff_to_send[0] = '\0';
    (*Clients)[user_number].buff[k - 1] = '\0';
}

void Accept_Client(User_Info** Clients, fd_set* descriptors, int* user_count, int main_descriptor, int* initial_ID){
    char send_buffer[128];
    int initial_fd;
    initial_fd = accept(main_descriptor, NULL, NULL);
    if (*user_count == SERVER_CAPACITY){
        printf("# User failed to connect the chat. Reason: server is full\n");
        if (send(initial_fd, ERROR_CAPACITY, strlen(ERROR_CAPACITY), 0) < 0) {
            printf("Ошибка отправки сообщения!\n");
        }
        shutdown(initial_fd, 2);
        close(initial_fd);
        return;
    }
    (*Clients)[*user_count].fd = initial_fd;
    (*Clients)[*user_count].buff = malloc(129 * sizeof(char));
    (*Clients)[*user_count].buff[0] = '\0';
    //(*Clients)[*user_count].buff_to_send = malloc(sizeof(char));
    //(*Clients)[*user_count].buff_to_send[0] = '\0';
    sprintf((*Clients)[*user_count].Username, "user_%d", *initial_ID);
    sprintf(send_buffer, "# %s joined the chat\r\n", (*Clients)[*user_count].Username);
    printf("%s", send_buffer);
    if (send((*Clients)[*user_count].fd, greeting, strlen(greeting), 0) < 0) {
        printf("Ошибка отправки сообщения!\n");
    }
    for (int i = 0; i < *user_count; i++) {
        if ((*Clients)[i].initial_named != 1) {
            if (send((*Clients)[i].fd, send_buffer, strlen(send_buffer), 0) < 0) {
                printf("Ошибка отправки сообщения!\n");
            }
        }
    }
    (*initial_ID)++;
    (*Clients)[*user_count].initial_named = 1;
    (*user_count)++;
}

void Making_Changes(User_Info** Clients, fd_set* descriptors, int* user_count, int main_descriptor, int* initial_ID) {
    char message_buffer[129];
    int size_message;
    if (FD_ISSET(main_descriptor, descriptors)) {
        Accept_Client(Clients, descriptors, user_count, main_descriptor, initial_ID);
    }
    for (int i = 0; i < *user_count; i++) {
        if (FD_ISSET((*Clients)[i].fd, descriptors)) {
            size_message = recv((*Clients)[i].fd, message_buffer, 128, 0);
            message_buffer[size_message] = '\0';
            if (size_message < 0) {
                printf("Ошибка получения сообщения!\n");
            } else if (size_message == 0) {
                Delete_Client(Clients, user_count, i);
            } else {
                Handle_Message(Clients, message_buffer, i, user_count);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (Check_Arguments(argc, argv[1]) == -1){
        printf("%s: Неверные аргументы\n", argv[0]);
        return 1;
    }
    int user_count = 0;
    int status;
    int initial_ID = 1;
    int max_descriptor;
    int error_port;
    struct timeval timeout;
    int Server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in bind_information;
    memset(&bind_information, 0, sizeof(bind_information));
    User_Info* Clients = malloc(15 * sizeof(User_Info));
    fd_set descriptors;
    bind_information.sin_family = AF_INET;
    bind_information.sin_addr.s_addr = htonl(INADDR_ANY);
    if (Server < 0){
        printf("Socket start error. Contact technical support.\n");
        return 1;
    }
    error_port = Connect_Port(&bind_information, atoi(argv[1]), Server);
    if (error_port == -1){
        printf("Недопустимый номер порта\n");
        shutdown(Server, 2);
        close(Server);
        return 1;
    } else if (error_port == -2) {
        printf("Ошибка привязки порта\n");
        shutdown(Server, 2);
        close(Server);
        return 1;
    }
    if (listen(Server, 5) < 0){
        printf("Невозможно прослушивать данный сокет!\n");
        shutdown(Server, 2);
        close(Server);
        return 1;
    }
    while (1){
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        max_descriptor = Update_Descriptors(Clients, &descriptors, user_count, Server);
        status = select(max_descriptor + 1, &descriptors, NULL, NULL, &timeout);
        if (status < 0){
            printf("Произошла неизвестная ошибка. Обратитесь в техническую поддержку\n");
            shutdown(Server, 2);
            close(Server);
        } else if (status == 0) {
            printf("# chat server is running... (%d user(s) connected)\n", user_count);
        } else {
            Making_Changes(&Clients, &descriptors, &user_count, Server, &initial_ID);
        }
    }
    return 0;
}
