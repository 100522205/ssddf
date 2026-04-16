#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include "sock.h"
#include "list.h"

List database=NULL; // variable para manejar base de datos
pthread_mutex_t mutex_db=PTHREAD_MUTEX_INITIALIZER;


void manejador_sigint(int sig) {
    printf("\n");
    toFile(database, "database.txt");
    destroy_list(&database);
    exit(0);
}


int error(char * message, int cod) {
    printf("\n Error en el código del servidor: %s\n", message);
    return cod;
}


void error_th(char * message) {
    printf("\n Error en el código del servidor: %s\n", message);
}


void logs(char* op, char* userName, char* res) {
    if (strcmp(op, "CONNECTEDUSERS")==0) printf("s> %s %s\n", op, res);
    else printf("s> %s %s %s\n", op, userName, res);
}


void message_logs(unsigned int id, char* sName, char* rName, int conn) {
    if (conn==1) printf("s> SEND MESSAGE %d FROM %s TO %s\n", id, sName, rName);
    else printf("s> MESSAGE %d FROM %s TO %s STORED\n", id, sName, rName);
}


int send_message(char* sName, char* rName, unsigned int id, char* msg) {
    /* 0. conectarse al thread */

    /* 1. enviar SEND_MESSAGE */

    /* 2. enviar sName */

    /* 3. enviar id */

    /* 4. enviar mensaje */

    /* 5. cerrar conexion */
}


int notify_message(char* userName, unsigned int id) {              // solo si esta conectado
    /* 0. conectarse al thread */

    /* 1. enviar SEND_MESS_ACK */

    /* 2. enviar id */

    /* 3. cerrar conexion */
}


struct thread_args {
    int     sd;
    char    ip[16];
};


void * worker(void * argum) {
    struct thread_args *args=(struct thread_args *)argum;
    int sd=args->sd;
    char ip[16]={0};
    strcpy(ip, args->ip);

    free(args);

    pthread_detach(pthread_self());

    char buff[LENG]={0};

    sock_receive(sd, buff, LENG);

    buff[LENG-1] = '\0';

    pthread_mutex_lock(&mutex_db);
    /* seccion criticia */

    char op[LENG]={0};
    strcpy(op, buff);

    if (strcmp(buff, "REGISTER")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_register;
        
        /* 1. verificar usuario no registrado */
        int exist=exists_in(database, userName);

        switch (exist)
        {
        /* 2a.1. almacenar usuario */
        case 0:
            if (setNode(&database, userName, 0)!=0) goto error_register;

        /* 2a.2. responder al cliente (0). guardar id de mnsj (0) */
            if (sock_send(sd, "0", 1)<0) goto error_register;
            // id guardado en 2a.1
            break;

        /* 2b. notificar error (1) : ya existe */
        case 1:
            if (sock_send(sd, "1", 1)<0) goto error_register;
            logs(op, userName, "FAIL");
            break;
        }

        goto end; // si llega aqui no ha habido "otros" errores

        /* 2c. notificar error (2) : otros*/
        error_register:
            sock_send(sd, "2", 1);
            logs(op, userName, "FAIL");
            goto finish;

        end:
            logs(op, userName, "OK");

    } else if (strcmp(buff, "UNREGISTER")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_unregister;

        /* 1. verificar usuario registrado */
        int exist=exists_in(database, userName);

        switch (exist)
        {
        /* 2a.1. borrar usuario */
        case 1:
            if (del_user(&database, userName)!=0) goto error_unregister;

        /* 2a.2. responder al cliente (0) */
            if (sock_send(sd, "0", 1)<0) goto error_unregister;
            break;

        /* 2b. notificar error (1) : no existe */
        case 0:
            if (sock_send(sd, "1", 1)<0) goto error_unregister;
            logs(op, userName, "FAIL");
            break;
        }

        goto end; // si llega aqui no ha habido "otros" errores

        /* 2c. notificar error (2) : otros */
        error_unregister:
            sock_send(sd, "2", 1);
            logs(op, userName, "FAIL");
            goto finish;

        /* 3. borrar mensajes pendientes */
        end:
            // mensajes borrados en 2a.1
            logs(op, userName, "OK");

    } else if (strcmp(buff, "CONNECT")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_connect;

        /* 1. recibir puerto */
        char port[LENG]={0};
        if (sock_receive(sd, port, LENG)<0) goto error_connect;

        /* 2. buscar usuario */
        int exist=exists_in(database, userName);
        uint8_t conn;
        if (get_conn(database, userName, &conn)!=0) goto error_connect;

        switch (conn)
        {
        case 0:
            switch (exist)
            {
        /* 2a.1. rellenar IP y puerto */
            case 1:
                if (set_ip_port(database, userName, ip, (uint16_t)atoi(port))!=0) goto error_connect;

        /* 2a.2. cambiar a conectado */
                if (modify_conn(database, userName, 1)!=0) goto error_connect; // 1 conn ; 0 disconn   

        /* 2a.3. responder al cliente (0) */
                if (sock_send(sd, "0", 1)<0) goto error_connect;

        /* 2a.4. enviar mensajes pendientes */
                int n=get_num_pending(database, userName);
                if (n==-1) goto error_connect;
                else if (n!=0)
                {
                    struct msgdata* pending=malloc(sizeof(struct msgdata) * n);

                    if (pending!=NULL)
                    {
                        if (get_mssg_pending(database, userName, pending)!=0) goto error_connect;

                        char sName[LENG]={0};
                        unsigned int id;
                        char msg[LENG]={0};
                        uint8_t connS;
                        for (int i=0; i<n; i++)
                        {
                            strcpy(sName, pending[i].sName);
                            id=pending[i].id;
                            strcpy(msg, pending[i].msg);

                            if (send_message(sName, userName, id, msg)!=0) goto error_connect;
                            if (del_mssg_pending(database, userName, sName, id)!=0) goto error_connect;
                            message_logs(id, sName, userName, 1);
                            if (get_conn(database, sName, &connS)!=0) goto error_connect;
                            if (connS==1) if (notify_message(sName, id)!=0) goto error_connect;   // enviar ACK si sender esta conectado                            
                        }
                        
                        free(pending);
                    }
                }
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                if (sock_send(sd, "1", 1)<0) goto error_connect;
                logs(op, userName, "FAIL");
                break;
            }
            break;

        /* 2c. notificar error (2) : ya conectado */
        case 1:
            if (sock_send(sd, "2", 1)<0) goto error_connect;
            logs(op, userName, "FAIL");
            break;
        }

        goto end; // si llega aqui no ha habido "otros" errores

        /* 2d. notificar error (3) : otros */
        error_connect:
            sock_send(sd, "3", 1);
            logs(op, userName, "FAIL");
            goto finish;
        
        end:
            logs(op, userName, "OK");

    } else if (strcmp(buff, "DISCONNECT")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_disconnect;

        /* 1. buscar usuario */
        int exist=exists_in(database, userName);
        uint8_t conn;
        if (get_conn(database, userName, &conn)!=0) goto error_disconnect;

        switch (conn)
        {
        case 1:
            switch (exist)
            {
        /* 2a.1. borrar IP y puerto */
            case 1:
                if (del_ip_port(database, userName)!=0) goto error_disconnect;
        /* 2a.2. cambiar a desconectado */
                if (modify_conn(database, userName, 0)!=0) goto error_disconnect; // 1 conn ; 0 disconn   

        /* 2a.3. responder al cliente (0) */
                if (sock_send(sd, "0", 1)<0) goto error_disconnect;
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                if (sock_send(sd, "1", 1)<0) goto error_disconnect;
                logs(op, userName, "FAIL");
                break;
            }
            break;

        /* 2c. notificar error (2) : no conectado */
        case 0:
            if (sock_send(sd, "2", 1)<0) goto error_disconnect;
            logs(op, userName, "FAIL");
            break;
        }
        
        goto end; // si llega aqui no ha habido "otros" errores

        /* 2d. notificar error (3) : otros */
        error_disconnect:
            sock_send(sd, "3", 1);
            logs(op, userName, "FAIL");
            goto finish;
        
        end:
            logs(op, userName, "OK");

    } else if (strcmp(buff, "SEND")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_send;

        /* 1. recibir nombre del destinatario */
        char rName[LENG]={0};
        if (sock_receive(sd, rName, LENG)<0) goto error_send;

        /* 2. recibir mensaje */
        char msg[LENG]={0};
        if (sock_receive(sd, msg, LENG)<0) goto error_send;

        /* 3. buscar usuario */
        int exist=exists_in(database, userName);

        switch (exist)
        {
        /* 4a. notificar error (1) : no existe */
        case 0:
            if (sock_send(sd, "1", 1)<0) goto error_send;
            logs(op, userName, "FAIL");
            break;

        /* 4b.1. almacenar mensaje */
        case 1:
            unsigned int id;
            if (get_id(database, userName, &id)!=0) goto error_send;
            if (add_mssg_pending(database, rName, userName, msg, id)!=0) goto error_send;

        /* 4b.a.1. enviar mensaje */
            uint8_t conn;
            uint8_t connU;
            if (get_conn(database, rName, &conn)!=0) goto error_send;
            if (conn==1)
            {
                if (send_message(userName, rName, id, msg)!=0) goto error_send;
                message_logs(id, userName, rName, conn);
                if (del_mssg_pending(database, rName, userName, id)!=0) goto error_send;
            }
            if (get_conn(database, userName, &connU)!=0) goto error_send;
            if (connU==1) if (notify_message(userName, id)!=0) goto error_send;   // enviar ACK si sender esta conectado                            

        /* 4b.a.2. responder al cliente (0). (id) */
                if (sock_send(sd, "0", 1)<0) goto error_send;
                if (sock_send(sd, (char*)&id, sizeof(unsigned int))<0) goto error_send;
                if (modify_id(database, userName)!=0) goto error_send;
                break;
        }

        goto finish; // si llega aqui no ha habido "otros" errores
        
        /* 5. notificar error (2) : otros */
        error_send:
            sock_send(sd, "2", 1);
            logs(op, userName, "FAIL");
            goto finish;

    } else if (strcmp(buff, "USERS")==0)
    {
        /* 1. buscar usuario */
        char userName[LENG]={0};
        uint8_t conn;
        int exist=exists_by_ip(database, ip, userName);
        if (get_conn(database, userName, &conn)!=0) goto error_users;

        /* 2a. notificar error (2) : no existe */
        if (exist==0)
        {
            if (sock_send(sd, "2", 1)<0) goto error_users;
            logs(op, userName, "FAIL");
            goto end;
        }

        /* 2b. notificar error (1) : no conectado */
        if (conn==0)
        {
            if (sock_send(sd, "1", 1)<0) goto error_users;
            logs(op, userName, "FAIL");
            goto end;
        }

        /* 2c.1. obtener usuarios conectados */
        int n;
        char* users[LENG]={0};
        for (int i = 0; i < LENG; i++) users[i] = malloc(LENG);
        if (get_users_conn(database, &n, users)!=0) goto error_users;

        /* 2c.2. responder al cliente (0). (#usr) */
        if (sock_send(sd, "0", 1)<0) goto error_users;
        int n_net = htonl(n);
        if (sock_send(sd, (char*)&n_net, sizeof(int))<0) goto error_users;

        /* 2c.3. enviar usuarios */
        for (int i=0; i<n; i++) if (sock_send(sd, users[i], LENG)<0) goto error_users;

        for (int i = 0; i < LENG; i++) free(users[i]);

        goto end; // si llega aqui no ha habido "otros" errores
        
        /* 5. notificar error (2) : otros */
        error_users:
            sock_send(sd, "2", 1);
            logs(op, userName, "FAIL");
            goto finish;

        end:
            logs(op, userName, "OK");

    }

    finish:

    /* fin seccion critica */
    pthread_mutex_unlock(&mutex_db);

    if(close(sd)<0){
        error_th("Error cerrando sd de conexión");
        }
    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    
    if (argc != 3) {
        printf("Usage: ./server -p <port>");
        exit(0);
    }

    if (strcmp(argv[1], "-p") != 0) {
        printf("Usage: ./server -p <port>\n");
        exit(0);
    }

    signal(SIGINT, manejador_sigint);

    // cargamos base de datos
    int res=fromFile(&database, "database.txt");
    if (res!=0) printf("Error al cargar base de datos\n");

    char maquina[256];
    if (gethostname(maquina, 256)==-1) return error("No se pudo obtener localIP", -2);

    struct sockaddr_in mySocket;
    struct hostent *ip=gethostbyname(maquina);
    int port=atoi(argv[2]);

    printf("s> init server %s:%d\n", inet_ntoa(*(struct in_addr*)ip->h_addr_list[0]), port);

    bzero(&mySocket, sizeof(struct sockaddr_in));
    mySocket.sin_addr.s_addr=INADDR_ANY;
    mySocket.sin_family=AF_INET;
    mySocket.sin_port=htons(port);

    int o=1;
    int server_sd=socket(AF_INET, SOCK_STREAM, 0);
    if(setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o))<0) return error("En setsockopt", -2);
    if(bind(server_sd, (struct sockaddr *)&mySocket, sizeof(mySocket))<0) return error("En bind", -2);

    if(listen(server_sd, SOMAXCONN)<0) return error("Error en listen", -2);

    printf("s>\n");

    /* recibir peticiones de clientes */
    while(1) {
        struct sockaddr_in socketRemote;
        socklen_t s=sizeof(socketRemote);

        int sd_client = accept(server_sd,(struct sockaddr *)&socketRemote, &s);

        if(sd_client<0) {
            printf("Error en accept");
            continue;
        }
        
        struct thread_args *args=malloc(sizeof(struct thread_args));
        if (args==NULL) {
            close(sd_client);
            continue;
        }

        args->sd=sd_client;
        inet_ntop(AF_INET, &(socketRemote.sin_addr), args->ip, 16);

        pthread_t id;
        pthread_create(&id, NULL, (void *)worker, args);
    }

    return -1;
}
