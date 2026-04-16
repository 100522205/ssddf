#ifndef _LISTA_H
#define _LISTA_H       
// Archivo .h
// Original de aula global
// Este es una derivacion de dicho archivo
#define LENG 255


struct msgdata {
	char 			sName[LENG];
	unsigned int	id;
	char			msg[LENG];
};


struct Node{ 
	char 			userName[LENG];
	uint8_t			conn;
	unsigned int	id;
	char			ip[16];
	uint16_t		port;
	struct msgdata	*pending;
	int				num_pending;
	struct Node		*next;
};


typedef struct Node * List;


int setNode(List* l, char* userName,  uint8_t conn);

int set_ip_port(List l, char* userName, char* ip, uint16_t port);

int get_conn(List l, char* userName,  uint8_t* conn);

int get_ip_port(List l, char* userName, char* ip, uint16_t* port);

int get_num_pending(List l, char* userName);

int get_mssg_pending(List l, char* userName, struct msgdata* pending);

int get_id(List l, char* userName, unsigned int* id);

int get_users_conn(List l, int* n, char** users);

int toFile(List l, char* name);

int fromFile(List* l, char* name);

int exists_in(List l, char* userName);

int exists_by_ip(List l, char* ip, char* userName);

int modify_conn(List l, char* userName,  uint8_t conn);

int modify_id(List l, char* userName);

int add_mssg_pending(List l, char* rName, char* sName, char* msg, unsigned int id);

int del_mssg_pending(List l, char* rName, char* sName, unsigned int id);

int del_ip_port(List l, char* userName);

int del_user(List* l, char* userName);

int destroy_list(List* l);

#endif

