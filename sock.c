#include "sock.h"


int sock_send(int fd_socket, char * buff, int size){
    /*Función para el envío de un string a un socket*/

    int remaining = size, sent;
    int to_return = 0;
    while((remaining>0)&&((sent =write(fd_socket, buff, remaining))>0)){
        remaining-=sent;
        to_return+=sent;
        buff+=sent;
    } 
    return to_return;

}


int sock_receive(int fd_socket, char*buff, int size){
    /* Función para la recepción de un string por socket*/

    int i = 0;
    char c;
    while (i < size - 1 && read(fd_socket, &c, 1) == 1) {
        if (c == '\0') break;
        buff[i++] = c;
    }
    buff[i] = '\0';
    return i;
}
