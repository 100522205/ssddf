#define _POSIX_C_SOURCE 200809L   // necesario para evitar errores con el manejador de interrupción de señal

#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "sock.h"
#include "list.h"

#define MAX_USERS 2048

List database=NULL; // variable para manejar base de datos
pthread_mutex_t mutex_db=PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t shutdown_flag = 0;


void manejador_sigint(int sig) { (void) sig; shutdown_flag = 1; }


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
    char ip[16];
    uint16_t port;
    if (get_ip_port(database, rName, ip, &port)!=0) return -1;

    int sd;
    struct sockaddr_in client_addr;
    
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) return -1;

    memset(&client_addr, 0, sizeof(client_addr));

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &(client_addr.sin_addr)) <= 0) return -1;

    if (connect(sd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) return -1;

    /* 1. enviar SEND_MESSAGE */
    if (sock_send(sd, "SEND_MESSAGE", 12)<0)
    {
        close(sd);
        return -1;
    }

    /* 2. enviar sName */
    if (sock_send(sd, sName, LENG)<0)
    {
        close(sd);
        return -1;
    }

    /* 3. enviar id */
    if (sock_send(sd, (char*)&id, sizeof(unsigned int))<0)
    {
        close(sd);
        return -1;
    }

    /* 4. enviar mensaje */
    if (sock_send(sd, msg, LENG)<0)
    {
        close(sd);
        return -1;
    }

    /* 5. cerrar conexion */
    close(sd);
    return 0;
}


int notify_message(char* userName, unsigned int id) {              // solo si esta conectado
    /* 0. conectarse al thread */
    char ip[16];
    uint16_t port;
    if (get_ip_port(database, userName, ip, &port)!=0) return -1;

    int sd;
    struct sockaddr_in client_addr;
    
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) return -1;

    memset(&client_addr, 0, sizeof(client_addr));

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &(client_addr.sin_addr)) <= 0) return -1;

    if (connect(sd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) return -1;

    /* 1. enviar SEND_MESS_ACK */
    if (sock_send(sd, "SEND_MESS_ACK", 13)<0)
    {
        close(sd);
        return -1;
    } 
    /* 2. enviar id */
    if (sock_send(sd, (char*)&id, sizeof(unsigned int))<0)
    {
        close(sd);
        return -1;
    }

    /* 3. cerrar conexion */
    close(sd);
    return 0;
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

    char op[LENG]={0};
    strcpy(op, buff);

    uint8_t ret_val;

    if (strcmp(buff, "REGISTER")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_register;
        
        /* 1. verificar usuario no registrado */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        switch (exist)
        {
        /* 2a.1. almacenar usuario */
        case 0:

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (setNode(&database, userName, 0)!=0) goto error_register;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

        /* 2a.2. responder al cliente (0). guardar id de mnsj (0) */
            ret_val=0;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_register;
            // id guardado en 2a.1
            break;

        /* 2b. notificar error (1) : ya existe */
        case 1:
            ret_val=1;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_register;
            logs(op, userName, "FAIL"); goto finish;
            break;
        }

        goto end_register; // si llega aqui no ha habido "otros" errores

        /* 2c. notificar error (2) : otros*/
        error_register:
            ret_val=2;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;

        end_register:
            logs(op, userName, "OK");

    } else if (strcmp(buff, "UNREGISTER")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_unregister;

        /* 1. verificar usuario registrado */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        switch (exist)
        {
        /* 2a.1. borrar usuario */
        case 1:

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (del_user(&database, userName)!=0) goto error_unregister;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

        /* 2a.2. responder al cliente (0) */
            ret_val=0;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_unregister;
            break;

        /* 2b. notificar error (1) : no existe */
        case 0:
            ret_val=1;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_unregister;
            logs(op, userName, "FAIL"); goto finish;
            break;
        }

        goto end_unregister; // si llega aqui no ha habido "otros" errores

        /* 2c. notificar error (2) : otros */
        error_unregister:
            ret_val=2;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;

        /* 3. borrar mensajes pendientes */
        end_unregister:
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

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        uint8_t conn;

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) goto error_connect;
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        switch (conn)
        {
        case 0:
            switch (exist)
            {
        /* 2a.1. rellenar IP y puerto */
            case 1:

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (set_ip_port(database, userName, ip, (uint16_t)atoi(port))!=0) goto error_connect;
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.2. cambiar a conectado */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (modify_conn(database, userName, 1)!=0) goto error_connect; // 1 conn ; 0 disconn   
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.3. responder al cliente (0) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_connect;

        /* 2a.4. enviar mensajes pendientes */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                int n=get_num_pending(database, userName);
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

                if (n==-1) goto error_connect;
                else if (n!=0)
                {
                    struct msgdata* pending=malloc(sizeof(struct msgdata) * n);

                    if (pending!=NULL)
                    {

                        pthread_mutex_lock(&mutex_db);
                        /* seccion criticia */
                        if (get_mssg_pending(database, userName, pending)!=0) goto error_connect;
                        /* fin seccion critica */
                        pthread_mutex_unlock(&mutex_db);

                        char sName[LENG]={0};
                        unsigned int id;
                        char msg[LENG]={0};
                        uint8_t connS;
                        for (int i=0; i<n; i++)
                        {
                            strcpy(sName, pending[i].sName);
                            id=pending[i].id;
                            strcpy(msg, pending[i].msg);

                            pthread_mutex_lock(&mutex_db);
                            /* seccion criticia */
                            if (send_message(sName, userName, id, msg)!=0) goto error_connect;
                            /* fin seccion critica */
                            pthread_mutex_unlock(&mutex_db);
                            
                            pthread_mutex_lock(&mutex_db);
                            /* seccion criticia */
                            if (del_mssg_pending(database, userName, sName, id)!=0) goto error_connect;
                            /* fin seccion critica */
                            pthread_mutex_unlock(&mutex_db);

                            message_logs(id, sName, userName, 1);

                            pthread_mutex_lock(&mutex_db);
                            /* seccion criticia */
                            if (get_conn(database, sName, &connS)!=0) goto error_connect;
                            /* fin seccion critica */
                            pthread_mutex_unlock(&mutex_db);

                            if (connS==1) if (notify_message(sName, id)!=0) goto error_connect;   // enviar ACK si sender esta conectado                            
                        }
                        
                        free(pending);
                    }
                }
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                ret_val=1;
                if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_connect;
                logs(op, userName, "FAIL"); goto finish;
                break;
            }
            break;

        /* 2c. notificar error (2) : ya conectado */
        case 1:
            ret_val=2;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_connect;
            logs(op, userName, "FAIL"); goto finish;
            break;
        }

        goto end_connect; // si llega aqui no ha habido "otros" errores

        /* 2d. notificar error (3) : otros */
        error_connect:
            ret_val=3;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;
        
        end_connect:
            logs(op, userName, "OK");

    } else if (strcmp(buff, "DISCONNECT")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) goto error_disconnect;

        /* 1. buscar usuario */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        if (exist == 0)     // este if evita mandar error_disconnect en vez de 1 si el usuario no existe
        {                   // porque la  llamada a get_conn fallaría
            ret_val=1;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_disconnect;
            logs(op, userName, "FAIL"); goto finish;
            goto end_disconnect;
        }

        uint8_t conn;

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) goto error_disconnect;
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        switch (conn)
        {
        case 1:
            switch (exist)
            {
        /* 2a.1. borrar IP y puerto */
            case 1:

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (del_ip_port(database, userName)!=0) goto error_disconnect;
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.2. cambiar a desconectado */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (modify_conn(database, userName, 0)!=0) goto error_disconnect; // 1 conn ; 0 disconn   
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.3. responder al cliente (0) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_disconnect;
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                ret_val=1;
                if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_disconnect;
                logs(op, userName, "FAIL"); goto finish;
                break;
            }
            break;

        /* 2c. notificar error (2) : no conectado */
        case 0:
            ret_val=2;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_disconnect;
            logs(op, userName, "FAIL"); goto finish;
            break;
        }
        
        goto end_disconnect; // si llega aqui no ha habido "otros" errores

        /* 2d. notificar error (3) : otros */
        error_disconnect:
            ret_val=3;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;
        
        end_disconnect:
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

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        switch (exist)
        {
        /* 4a. notificar error (1) : no existe */
        case 0:
            ret_val=1;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_send;
            logs(op, userName, "FAIL"); goto finish;
            break;

        /* 4b.1. almacenar mensaje */
        case 1:
            unsigned int id;

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (get_id(database, userName, &id)!=0) goto error_send;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (add_mssg_pending(database, rName, userName, msg, id)!=0) goto error_send;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

        /* 4b.a.1. enviar mensaje */
            uint8_t conn;
            uint8_t connU;

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (get_conn(database, rName, &conn)!=0) goto error_send;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

            if (conn==1)
            {
                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (send_message(userName, rName, id, msg)!=0) goto error_send;
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);
                message_logs(id, userName, rName, conn);

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (del_mssg_pending(database, rName, userName, id)!=0) goto error_send;
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

            } else
            {
                message_logs(id, userName, rName, conn);
            }

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (get_conn(database, userName, &connU)!=0) goto error_send;
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

            if (connU==1) if (notify_message(userName, id)!=0) goto error_send;   // enviar ACK si sender esta conectado                            

        /* 4b.a.2. responder al cliente (0). (id) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_send;
                if (sock_send(sd, (char*)&id, sizeof(unsigned int))<0) goto error_send;

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (modify_id(database, userName)!=0) goto error_send;
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

                break;
        }

        goto finish; // si llega aqui no ha habido "otros" errores
        
        /* 5. notificar error (2) : otros */
        error_send:
            ret_val=2;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;

    } else if (strcmp(buff, "USERS")==0)
    {
        /* 1. buscar usuario */
        char userName[LENG]={0};
        uint8_t conn;
        if (sock_receive(sd, userName, LENG)<0) goto error_send;

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) goto error_users;
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        /* 2a. notificar error (2) : no existe */
        if (exist==0)
        {
            ret_val=2;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_users;
            logs(op, userName, "FAIL"); goto finish;
            goto end_users;
        }

        /* 2b. notificar error (1) : no conectado */
        if (conn==0)
        {
            ret_val=1;
            if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_users;
            logs(op, userName, "FAIL"); goto finish;
            goto end_users;
        }

        /* 2c.1. obtener usuarios conectados */
        int n;
        char* users[MAX_USERS]={0};
        for (int i = 0; i < LENG; i++) users[i] = malloc(LENG);

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_users_conn(database, &n, users)!=0) goto error_users;
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        /* 2c.2. responder al cliente (0). (#usr) */
        ret_val=0;
        if (sock_send(sd, (char*)&ret_val, 1)<0) goto error_users;
        int n_net = htonl(n);
        if (sock_send(sd, (char*)&n_net, sizeof(int))<0) goto error_users;

        /* 2c.3. enviar usuarios */
        for (int i=0; i<n; i++) if (sock_send(sd, users[i], LENG)<0) goto error_users;

        for (int i = 0; i < LENG; i++) free(users[i]);

        goto end_users; // si llega aqui no ha habido "otros" errores
        
        /* 5. notificar error (2) : otros */
        error_users:
            for (int i = 0; i < LENG; i++) if (users[i]) free(users[i]);
            ret_val=2;
            sock_send(sd, (char*)&ret_val, 1);
            logs(op, userName, "FAIL"); goto finish;
            goto finish;

        end_users:
            logs(op, userName, "OK");

    }

    finish:

    if(close(sd)<0){
        error_th("Error cerrando sd de conexión");
        }
    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    
    if (argc != 3) {
        printf("Usage: ./server -p <port>\n");
        exit(0);
    }

    if (strcmp(argv[1], "-p") != 0) {
        printf("Usage: ./server -p <port>\n");
        exit(0);
    }

    struct sigaction sa;
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // cargamos base de datos
    int res=fromFile(&database, "database.json");
    if (res!=0) printf("Error al cargar base de datos\n");

    char maquina[256];
    if (gethostname(maquina, 256)==-1) return error("No se pudo obtener localIP", -2);

    struct sockaddr_in mySocket;
    struct hostent *ip=gethostbyname(maquina);
    int port=atoi(argv[2]);

    printf("s> init server %s:%d\n", inet_ntoa(*(struct in_addr*)ip->h_addr_list[0]), port);

    memset(&mySocket, 0, sizeof(struct sockaddr_in));
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
    while(!shutdown_flag) {
        struct sockaddr_in socketRemote;
        socklen_t s=sizeof(socketRemote);

        int sd_client = accept(server_sd,(struct sockaddr *)&socketRemote, &s);

        if(sd_client<0) {
            if (errno == EINTR) break;
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

    toFile(database, "database.json");

    return -1;
}
