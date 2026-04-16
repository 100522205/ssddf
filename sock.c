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

    int remaining = size, read_v, to_return=0;
    while((remaining>0)&&((read_v = read(fd_socket, buff, remaining))>0)){
        remaining-=read_v;
        to_return+=read_v;
        buff+=read_v;
    }
    return to_return;
}
