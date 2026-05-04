#define _POSIX_C_SOURCE 200809L   // necesario para evitar errores con el manejador de interrupción de señal

#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "sock.h"
#include "list.h"

// para el logger con rpc

#include "logger.h"

void call_log(char * uName, char*op, char*file){
    // primero de nada, ver si tenemos variable de entorno

    char * ip_rpc = getenv("LOG_RPC_IP");
    // y si no la dan? localhost

    if(ip_rpc==NULL) ip_rpc="localhost";

    CLIENT * clnt = clnt_create(ip_rpc, LOGGER, LOGGERVER, "udp");
    if(clnt==NULL) return;

    struct log_strct args;
    strncpy(args.uName, uName, 256);
    strncpy(args.op, op, 256);

    // hay que tener en cuenta que TAL VEZ NO HAY FILE
    strncpy(args.file, file ? file : "", 256);

    int res;
    log_1(args, &res, clnt);
    clnt_destroy(clnt);
}

#define MAX_USERS 2048

List database=NULL; // variable para manejar base de datos
pthread_mutex_t mutex_db=PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t shutdown_flag = 0;


int send_message_attach(char* sName, char* rName, unsigned int id, char* msg, char*file){
    // funcion para el envio de mensajes cuando tenemos un file
    char ip[16];
    uint16_t port;
    if(get_ip_port(database, rName, ip, &port)!=0) return -1;

    int sd=socket(AF_INET, SOCK_STREAM,0);
    if(sd<0) return -1;

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family=AF_INET;
    client_addr.sin_port=htons(port);

    if(inet_pton(AF_INET, ip, &(client_addr.sin_addr))<=0){
        close(sd);
        return -1;
    }

    if(connect(sd, (struct sockaddr*)&client_addr, sizeof(client_addr))<0){
        close(sd);
        return -1;
    }

    if(sock_send(sd, "SEND_MESSAGE_ATTACH", strlen("SEND_MESSAGE_ATTACH")+1)<0){
        close(sd);
        return -1;
    }

    if(sock_send(sd, sName, strlen(sName)+1)<0){
        close(sd);
        return -1;
    }

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%u", id);
    if(sock_send(sd, id_str, strlen(id_str)+1)<0) {
        close(sd);
        return -1;
    }

    if(sock_send(sd, msg, strlen(msg)+1)<0) {
        close(sd);
        return -1;
    }

    if(sock_send(sd, file, strlen(file)+1)<0) {
        close(sd);
        return -1;
    }

    close(sd);
    return 0;
}

void manejador_sigint(int sig) { (void) sig; shutdown_flag = 1; }

int notify_message_attach(char* userName, unsigned int id, char*file){
    char ip[16];
    uint16_t port;

    // comprobar si podemos obtener puerto
    if(get_ip_port(database, userName, ip, &port)!=0) return -1;

    int sd=socket(AF_INET, SOCK_STREAM, 0);
    if(sd<0) return -1;

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family=AF_INET;
    client_addr.sin_port=htons(port);

    if(inet_pton(AF_INET, ip, &(client_addr.sin_addr))<=0){
        close(sd);
        return -1;
    }

    if(connect(sd, (struct sockaddr*)&client_addr, sizeof(client_addr))<0){
        close(sd);
        return -1;
    }

    if(sock_send(sd, "SEND_MESS_ATTACH_ACK", strlen("SEND_MESS_ATTACH_ACK") +1)<0){
        close(sd);
        return -1;
    }

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%u", id);
    if(sock_send(sd, id_str, strlen(id_str)+1)<0){
        close(sd);
        return -1;
    }

    if(sock_send(sd, file, strlen(file)+1)<0){
        close(sd);
        return -1;
    }

    close(sd);
    return 0;

}

int error(char * message, int cod) {
    printf("\n Error en el código del servidor: %s\n", message);
    return cod;
}


void error_th(char * message) {
    printf("\n Error en el código del servidor: %s\n", message);
}


void logs(char* op, char* userName, char* res) {
    if (strcmp(op, "USERS")==0) printf("s> %s %s\n", op, res);
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

    if (inet_pton(AF_INET, ip, &(client_addr.sin_addr)) <= 0) {
        close(sd);
        return -1;}

    if (connect(sd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        close(sd);
        return -1;}

    /* 1. enviar SEND_MESSAGE */
    if (sock_send(sd, "SEND_MESSAGE", 13)<0)
    {
        close(sd);
        return -1;
    }

    /* 2. enviar sName */
    if (sock_send(sd, sName, strlen(sName)+1)<0)
    {
        close(sd);
        return -1;
    }

    /* 3. enviar id */
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", id);
    if (sock_send(sd, id_str, strlen(id_str)+1)<0)
    {
        close(sd);
        return -1;
    }

    /* 4. enviar mensaje */
    if (sock_send(sd, msg, strlen(msg)+1)<0)
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
    /* 2. enviar id PERO COMO STRING */
    
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%u", id);
    if(sock_send(sd, id_str, strlen(id_str)+1)<0){
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

void exitWorker(int sd){
    if(close(sd)<0) error_th("Error cerrando sd");
    pthread_exit(NULL);
}

void errOut(uint8_t err_val, int sd, char * op, char * userName){
    sock_send(sd, (char *)&err_val, 1);
    logs(op,userName, "FAIL");
    exitWorker(sd); // cierra socket, termina el hilo
}


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
        if (sock_receive(sd, userName, LENG)<0) errOut(2, sd, op, userName);
        
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
            if (setNode(&database, userName, 0)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

        /* 2a.2. responder al cliente (0). guardar id de mnsj (0) */
            ret_val=0;
            if (sock_send(sd, (char*)&ret_val, 1)<0) errOut(2, sd, op, userName);
            // id guardado en 2a.1
            break;

        /* 2b. notificar error (1) : ya existe */
        case 1:
            errOut(1,sd,op,userName);
            break;
        }

        /* 2c. notificar error (2) : otros*/
        logs(op,userName,"OK");

        // uso de RPC
        call_log(userName, "REGISTER", NULL);

    } else if (strcmp(buff, "UNREGISTER")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) errOut(2, sd, op, userName);

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
            if (del_user(&database, userName)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

        /* 2a.2. responder al cliente (0) */
            ret_val=0;
            if (sock_send(sd, (char*)&ret_val, 1)<0) errOut(2, sd, op, userName);
            break;

        /* 2b. notificar error (1) : no existe */
        case 0:
            errOut(1,sd,op,userName);
            break;
        }
        
        logs(op, userName, "OK");
        // uso de RPC
        call_log(userName, "UNREGISTER", NULL);

    } else if (strcmp(buff, "CONNECT")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) errOut(3, sd, op, userName);

        /* 1. recibir puerto */
        char port[LENG]={0};
        if (sock_receive(sd, port, LENG)<0) errOut(3, sd, op, userName);

        /* 2. buscar usuario */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        uint8_t conn;

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) {pthread_mutex_unlock(&mutex_db);  errOut(3, sd, op, userName);}
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
                if (set_ip_port(database, userName, ip, (uint16_t)atoi(port))!=0) {pthread_mutex_unlock(&mutex_db); errOut(3, sd, op, userName);}
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.2. cambiar a conectado */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (modify_conn(database, userName, 1)!=0) {pthread_mutex_unlock(&mutex_db); errOut(3, sd, op, userName);} // 1 conn ; 0 disconn   
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.3. responder al cliente (0) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) errOut(3, sd, op, userName);

        /* 2a.4. enviar mensajes pendientes */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                int n=get_num_pending(database, userName);
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

                if (n==-1) errOut(3, sd, op, userName);
                else if (n!=0)
                {
                    struct msgdata* pending=malloc(sizeof(struct msgdata) * n);

                    if (pending!=NULL)
                    {

                        pthread_mutex_lock(&mutex_db);
                        /* seccion criticia */
                        if (get_mssg_pending(database, userName, pending)!=0) {pthread_mutex_unlock(&mutex_db); free(pending); errOut(3, sd, op, userName);}
                        /* fin seccion critica */
                        pthread_mutex_unlock(&mutex_db);

                        for (int i=0; i<n; i++)
                        {
                            int res_send=-1;

                            if(strlen(pending[i].file)>0){
                                res_send = send_message_attach(pending[i].sName, userName, pending[i].id, pending[i].msg, pending[i].file);
                                if(res_send==0)
                                    notify_message_attach(pending[i].sName, pending[i].id, pending[i].file);
                            } else{
                                res_send = send_message(pending[i].sName, userName, pending[i].id, pending[i].msg);
                                if(res_send==0) notify_message(pending[i].sName, pending[i].id);
                            }
                            
                            // si el envio fue exitoso, borrar de pending
                            if(res_send==0){
                                pthread_mutex_lock(&mutex_db);
                                del_mssg_pending(database, userName, pending[i].sName, pending[i].id);
                                pthread_mutex_unlock(&mutex_db);
                                message_logs(pending[i].id, pending[i].sName, userName, 1);
                            }
                        }
                        
                        free(pending);
                    }
                }
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                errOut(1,sd,op,userName);
                break;
            }
            break;

        /* 2c. notificar error (2) : ya conectado */
        case 1:
            errOut(2,sd,op,userName);
            break;
        }
        
        logs(op, userName, "OK");
        // uso de RPC
        call_log(userName, "CONNECT", NULL);

    } else if (strcmp(buff, "DISCONNECT")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) errOut(3, sd, op, userName);

        /* 1. buscar usuario */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        if (exist == 0)     // este if evita mandar error_disconnect en vez de 1 si el usuario no existe
        {                   // porque la  llamada a get_conn fallaría
            errOut(1,sd,op,userName);
        }

        uint8_t conn;

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) {pthread_mutex_unlock(&mutex_db); errOut(3, sd, op, userName);}
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
                if (del_ip_port(database, userName)!=0) {pthread_mutex_unlock(&mutex_db); errOut(3, sd, op, userName);}
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.2. cambiar a desconectado */

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (modify_conn(database, userName, 0)!=0) {pthread_mutex_unlock(&mutex_db); errOut(3, sd, op, userName);} // 1 conn ; 0 disconn   
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

        /* 2a.3. responder al cliente (0) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) errOut(3, sd, op, userName);
                break;

        /* 2b. notificar error (1) : no existe */
            case 0:
                errOut(1,sd,op,userName);
                break;
            }
            break;

        /* 2c. notificar error (2) : no conectado */
        case 0:
            errOut(2,sd,op,userName);
            break;
        }
        
        logs(op, userName, "OK");
        // uso de RPC
        call_log(userName, "DISCONNECT", NULL);

    } else if (strcmp(buff, "SEND")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) errOut(2, sd, op, userName);

        /* 1. recibir nombre del destinatario */
        char rName[LENG]={0};
        if (sock_receive(sd, rName, LENG)<0) errOut(2, sd, op, userName);

        /* 2. recibir mensaje */
        char msg[LENG]={0};
        if (sock_receive(sd, msg, LENG)<0) errOut(2, sd, op, userName);

        /* 3. buscar usuario */

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        int existr=exists_in(database, rName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        if (exist==0 || existr==0) exist=0;

        switch (exist)
        {
        /* 4a. notificar error (1) : no existe */
        case 0:
            errOut(1,sd,op,userName);
            break;

        /* 4b.1. almacenar mensaje */
        case 1:
            unsigned int id;

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (modify_id(database, userName)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}

            if (get_id(database, userName, &id)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}

            if (add_mssg_pending(database, rName, userName, msg, id, NULL)!=0) {pthread_mutex_unlock(&mutex_db);  errOut(2, sd, op, userName);}


        /* 4b.a.1. enviar mensaje */
            uint8_t conn;
            uint8_t connU;
            if (get_conn(database, rName, &conn)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

            if (conn==1)
            {
                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (send_message(userName, rName, id, msg)!=0) {
                    pthread_mutex_unlock(&mutex_db); 
                    errOut(2, sd, op, userName);
                }
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);
                message_logs(id, userName, rName, conn);

                pthread_mutex_lock(&mutex_db);
                /* seccion criticia */
                if (del_mssg_pending(database, rName, userName, id)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
                /* fin seccion critica */
                pthread_mutex_unlock(&mutex_db);

            } else
            {
                message_logs(id, userName, rName, conn);
            }

            pthread_mutex_lock(&mutex_db);
            /* seccion criticia */
            if (get_conn(database, userName, &connU)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
            /* fin seccion critica */
            pthread_mutex_unlock(&mutex_db);

            if (connU==1) if (notify_message(userName, id)!=0) errOut(2, sd, op, userName);   // enviar ACK si sender esta conectado                            

        /* 4b.a.2. responder al cliente (0). (id) */
                ret_val=0;
                if (sock_send(sd, (char*)&ret_val, 1)<0) errOut(2, sd, op, userName);
                
                char id_str[32];
                snprintf(id_str, sizeof(id_str), "%u", id);
                sock_send(sd,id_str, sizeof(id_str)+1);

                break;
        }
        // uso de RPC
        call_log(userName, "SEND", NULL);

    } else if (strcmp(buff, "SENDATTACH")==0)
    {
        /* 0. recibir nombre de usuario */
        char userName[LENG]={0};
        if (sock_receive(sd, userName, LENG)<0) errOut(2, sd, op, userName);

        /* 1. recibir nombre del destinatario */
        char rName[LENG]={0};
        if (sock_receive(sd, rName, LENG)<0) errOut(2, sd, op, userName);

        /* 2. recibir mensaje */
        char msg[LENG]={0};
        if (sock_receive(sd, msg, LENG)<0) errOut(2, sd, op, userName);

        /* 3. buscar el file (nombre de fichero)*/

        char file[LENG]={0};
        if (sock_receive(sd, file, LENG)<0) errOut(2, sd, op, userName);

        /* 4. buscar usuario */

        pthread_mutex_lock(&mutex_db);

        /* seccion criticia */
        modify_id(database, userName); // incrementar id antes

        unsigned int msg_id;
        get_id(database, userName, &msg_id);
        add_mssg_pending(database, rName, userName, msg, msg_id, file);
        /* fin seccion critica */

        pthread_mutex_unlock(&mutex_db);

        // responder exito al cliente
        ret_val=0;
        sock_send(sd, (char *)&ret_val, 1);
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%u", msg_id);
        sock_send(sd, id_str, strlen(id_str)+1);

        // intentar la entrega inmediata

        pthread_mutex_lock(&mutex_db);
        uint8_t rConn;
        get_conn(database, rName, &rConn);

        if(rConn==1){
            send_message_attach(userName, rName, msg_id, msg, file);
            notify_message_attach(userName, msg_id, file);
            del_mssg_pending(database, rName, userName, msg_id);
        }

        pthread_mutex_unlock(&mutex_db);

        // uso de RPC
        call_log(userName, "SENDATTACH", file);

    } else if (strcmp(buff, "USERS")==0)
    {
        /* 1. buscar usuario */
        char userName[LENG]={0};
        uint8_t conn;
        if (sock_receive(sd, userName, LENG)<0) errOut(2, sd, op, userName);

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int exist=exists_in(database, userName);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        char* users[MAX_USERS]={0}; // evita errores al hacer free en error_users        

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        if (get_conn(database, userName, &conn)!=0) {pthread_mutex_unlock(&mutex_db); errOut(2, sd, op, userName);}
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        /* 2a. notificar error (2) : no existe */
        if (exist==0) errOut(2,sd,op,userName);

        /* 2b. notificar error (1) : no conectado */
        if (conn==0) errOut(1,sd,op,userName);

        /* 2c.1. obtener usuarios conectados */
        int n;
        for (int i = 0; i < MAX_USERS; i++) users[i] = malloc(LENG);

        pthread_mutex_lock(&mutex_db);
        /* seccion criticia */
        int res_users = get_users_conn(database, &n, users);
        /* fin seccion critica */
        pthread_mutex_unlock(&mutex_db);

        if (res_users!=0) {
            for(int j=0; j<MAX_USERS; ++j){
                free(users[j]);
            }
            errOut(2, sd, op, userName);
        }

        /* 2c.2. responder al cliente (0). (#usr) */
        ret_val=0;
        if (sock_send(sd, (char*)&ret_val, 1)<0) {
            for(int j=0; j<MAX_USERS; ++j){
                free(users[j]);
            }
            errOut(2, sd, op, userName);
        }

        int n_net = htonl(n);
        if (sock_send(sd, (char*)&n_net, sizeof(int))<0) {
            for(int j=0; j<MAX_USERS; ++j){
                free(users[j]);
            }
            errOut(2, sd, op, userName);}

        /* 2c.3. enviar usuarios */
        for (int i=0; i<n; i++) if (sock_send(sd, users[i], LENG)<0) {
            for(int j=0; j<MAX_USERS; ++j){
                free(users[j]);
            }
            errOut(2, sd, op, userName);}

        for (int i = 0; i < MAX_USERS; i++) free(users[i]);        
        /* 5. notificar error (2) : otros */
        logs(op, userName, "OK");
        // uso de RPC
        call_log(userName, "USERS", NULL);

    }

    exitWorker(sd);
    return NULL;
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
    close(server_sd);

    return -1;
}
