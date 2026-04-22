#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "list.h"
// Archivo .c de lista
// Original de aula global
// Este es una derivacion de dicho archivo

int setNode(List* l, char* userName, uint8_t conn) {
    struct Node * newNode = (struct Node *)malloc(sizeof(struct Node));
    if(newNode == NULL) return -1;

    strncpy(newNode->userName, userName, LENG);
    newNode->userName[LENG - 1] = '\0';

    newNode->conn = conn;

	newNode->id = 0;

    memset(newNode->ip, 0, 16);
    newNode->port = 0;
    
    newNode->pending = NULL;
    newNode->num_pending = 0;

    newNode->next = *l;
    *l = newNode;

    return 0;
}


int set_ip_port(List l, char* userName, char* ip, uint16_t port) {
    if (l == NULL) return -1;

    struct Node *aux = l;
    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            // Copiamos la IP de forma segura (máximo 15 carácteres + \0)
            strncpy(aux->ip, ip, 16);
            aux->ip[15] = '\0'; // Aseguramos el cierre del string
            
            aux->port = port;
            
            return 0; // Encontrado y modificado
        }
        aux = aux->next;
    }
    return -1;
}


int get_conn(List l, char* userName,  uint8_t* conn) {
	// funcion para copiar un nodo de la lista (obtenerlo o get)
	// según una key
	List aux;

	aux = l;	

	while (aux!=NULL) {
		if (strcmp(aux->userName, userName)==0) {
			// el get en si
			*conn=aux->conn;
			return(0);		// found
		}
		else
			aux = aux->next;
	}

	return -1;  // not found
}	


int get_ip_port(List l, char* userName, char* ip, uint16_t* port) {
    List aux = l;

    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            strncpy(ip, aux->ip, 16);
            ip[15] = '\0';
            *port = aux->port;
            return 0;   // found
        }
        aux = aux->next;
    }

    return -1;  // not found
}


int get_num_pending(List l, char* userName) {
    /* Devuelve el número de mensajes o -1 si el usuario no existe */
    List aux = l;

    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            return aux->num_pending;
        }
        aux = aux->next;
    }

    return -1; // Usuario no encontrado
}


int get_mssg_pending(List l, char* userName, struct msgdata* pending) {
    /* Popula el array y devuelve 0 (éxito) o -1 (error) */
    if (pending == NULL) return -1;

    List aux = l;
    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            // Si hay mensajes, los copiamos al array destino
            if (aux->num_pending > 0 && aux->pending != NULL) {
                memcpy(pending, aux->pending, sizeof(struct msgdata) * aux->num_pending);
            }
            // Si num_pending es 0, no hace falta copiar nada, es un éxito "vacío"
            return 0; 
        }
        aux = aux->next;
    }

    return -1; // Usuario no encontrado
}


int get_id(List l, char* userName, unsigned int* id) {
    /* Funcion para obtener el id actual de un usuario */
    if (l == NULL || id == NULL) return -1;

    List aux = l;

    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            *id = aux->id;
            return 0;   // Éxito: usuario encontrado
        }
        aux = aux->next;
    }

    return -1;  // Error: usuario no encontrado
}


int get_users_conn(List l, int* n, char** users) {
    /* Funcion para obtener el numero y los nombres de usuarios conectados */
    if (l == NULL || n == NULL || users == NULL) return -1;

    int count = 0;
    struct Node *aux = l;

    // Recorremos la lista buscando usuarios con conn == 1
    while (aux != NULL) {
        if (aux->conn == 1) {
            // Copiamos el userName al array de punteros
            strncpy(users[count], aux->userName, LENG);
            users[count][LENG - 1] = '\0';
            
            count++;
        }
        aux = aux->next;
    }

    // Guardamos el total en el puntero n
    *n = count;

    return 0; // Exito
}


int toFile(List l, char * name) {
    FILE * fichero = fopen(name, "wb");
    if(fichero == NULL) return -1;

    struct Node * aux = l;

    while(aux != NULL) {
        fwrite(aux->userName, sizeof(char), LENG, fichero);
        fwrite(&(aux->conn), sizeof(uint8_t), 1, fichero);
		fwrite(&(aux->id), sizeof(unsigned int), 1, fichero);
        
        fwrite(aux->ip, sizeof(char), 16, fichero);
        fwrite(&(aux->port), sizeof(uint16_t), 1, fichero);

        int n_msg = 0;
        if (aux->pending != NULL) {
            n_msg = aux->num_pending;
        }

        fwrite(&n_msg, sizeof(int), 1, fichero);

        if (n_msg > 0 && aux->pending != NULL) {
            fwrite(aux->pending, sizeof(struct msgdata), n_msg, fichero);
        }

        aux = aux->next;
    }

    fclose(fichero);
    return 0;
}


int fromFile(List * l, char * name) {
    FILE * fichero = fopen(name, "rb");
    if (fichero == NULL) return -1;

    destroy_list(l);

    char uName[LENG];
    uint8_t connection;
	unsigned int id;
    char ipAddr[16];
    uint16_t portNum;
    int n_msg;
    struct msgdata temp_msg;

    while (fread(uName, sizeof(char), LENG, fichero) == LENG) {

        if (fread(&connection, sizeof(uint8_t), 1, fichero) != 1) goto error;
		if (fread(&id, sizeof(unsigned int), 1, fichero) != 1) goto error;
        if (fread(ipAddr, sizeof(char), 16, fichero) != 16) goto error;
        if (fread(&portNum, sizeof(uint16_t), 1, fichero) != 1) goto error;
        if (fread(&n_msg, sizeof(int), 1, fichero) != 1) goto error;

        if (setNode(l, uName, connection) != 0) goto error;
        set_ip_port(*l, uName, ipAddr, portNum);

        for (int i = 0; i < n_msg; i++) {
            if (fread(&temp_msg, sizeof(struct msgdata), 1, fichero) != 1) goto error;
            add_mssg_pending(*l, uName, temp_msg.sName, temp_msg.msg, temp_msg.id);
        }
    }

    fclose(fichero);
    return 0;

error:
    fclose(fichero);
    destroy_list(l);
    return -1;
}


int exists_in(List l, char* userName) {
	/* Funcion para ver si existe un elemento en la lista dada una key*/

	if (l == NULL)  // lista vacia
		return 0;
	
	List aux = l;
	while (aux!=NULL) {
		if (strcmp(aux->userName, userName)==0) {
			return 1;		// found
		}
		aux = aux->next;
	}

	return 0;
}


int modify_conn(List l, char* userName, uint8_t conn) {
    if (l == NULL) return -1; // Lista vacía

    struct Node *aux = l;
    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            aux->conn = conn;
            return 0; // Encontrado y modificado
        }
        aux = aux->next;
    }
    return -1; // No encontrado
}


int modify_id(List l, char* userName) {
    if (l == NULL) return -1;

    struct Node *aux = l;
    
    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            
            // Si el id ya es el máximo valor de un unsigned int
            if (aux->id == UINT_MAX) {
                aux->id = 0; // Reiniciamos a 0
            } else {
                aux->id++;   // Incremento normal
            }

            return 0; 
        }
        aux = aux->next;
    }

    return -1;
}


int add_mssg_pending(List l, char* rName, char* sName, char* msg, unsigned int id) {
    if (l == NULL) return -1;

    struct Node *aux = l;
    while (aux != NULL) {
        if (strcmp(aux->userName, rName) == 0) {
            // 1. Intentar ampliar la memoria para un mensaje más
            struct msgdata *temp = realloc(aux->pending, sizeof(struct msgdata) * (aux->num_pending + 1));
            
            if (temp == NULL) return -1; // Fallo de memoria

            aux->pending = temp;

            // 2. Rellenar los datos en la última posición
            struct msgdata *nuevo = &aux->pending[aux->num_pending];
            strncpy(nuevo->sName, sName, LENG);
            nuevo->sName[LENG - 1] = '\0';
            
            strncpy(nuevo->msg, msg, LENG);
            nuevo->msg[LENG - 1] = '\0';
            
            nuevo->id = id;

            // 3. Incrementar el contador
            aux->num_pending++;
            return 0;
        }
        aux = aux->next;
    }
    return -1; // Usuario no encontrado
}


int del_mssg_pending(List l, char* rName, char* sName, unsigned int id) {
    if (l == NULL) return -1;

    struct Node *aux = l;
    // 1. Buscamos al nodo receptor (rName)
    while (aux != NULL) {
        if (strcmp(aux->userName, rName) == 0) {
            int found_idx = -1;

            // 2. Buscamos el mensaje que coincida con sName E id
            for (int i = 0; i < aux->num_pending; i++) {
                if (aux->pending[i].id == id && strcmp(aux->pending[i].sName, sName) == 0) {
                    found_idx = i;
                    break;
                }
            }

            // Si no se encuentra el mensaje exacto
            if (found_idx == -1) return -1;

            // 3. Desplazar los elementos a la izquierda para "tapar" el hueco
            for (int i = found_idx; i < aux->num_pending - 1; i++) {
                aux->pending[i] = aux->pending[i + 1];
            }

            // 4. Actualizar contador
            aux->num_pending--;

            // 5. Ajustar memoria dinámica
            if (aux->num_pending == 0) {
                // Si era el último, liberamos el bloque
                free(aux->pending);
                aux->pending = NULL;
            } else {
                // Redimensionamos a un tamaño menor
                struct msgdata *temp = realloc(aux->pending, sizeof(struct msgdata) * aux->num_pending);
                if (temp != NULL) {
                    aux->pending = temp;
                }
            }
            return 0; // Éxito
        }
        aux = aux->next;
    }
    return -1; // Usuario receptor no encontrado
}


int del_ip_port(List l, char* userName) {
    if (l == NULL) return -1;

    struct Node *aux = l;
    while (aux != NULL) {
        if (strcmp(aux->userName, userName) == 0) {
            memset(aux->ip, 0, 16);
            
            aux->port = 0;
            
            return 0; // Usuario encontrado y datos limpiados
        }
        aux = aux->next;
    }
    return -1; // Usuario no encontrado
}


int del_user(List* l, char* userName) {
	// Funcion para eliminar de la lista un nodo que tenga una key
	List aux, back;

	if (*l == NULL)  // lista vacia
		return -1;

	// primer elemento de la lista
	if (strcmp(userName, (*l)->userName) == 0){
		aux = *l;
		*l = (*l)->next;
		if (aux->pending != NULL) free(aux->pending);
		free(aux);
		return 0;
	}
	
	back = *l;
	aux = (*l)->next;
	while (aux!=NULL) {
		if (strcmp(aux->userName, userName)==0) {
			back->next = aux->next;
			if (aux->pending != NULL) free(aux->pending);
			free(aux);
			return 0;		// found
		}
		else {
			back = aux;
			aux = aux->next;
		}
	}

	return -1;
}	


int destroy_list(List *l){
	// elimina la lista de manera segura
	List aux; 

	while (*l != NULL){
		aux = *l;
		*l = aux->next;
		if (aux->pending != NULL) free(aux->pending);
		free(aux);
	}

	return 0;
}	

