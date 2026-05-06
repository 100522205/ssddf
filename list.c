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
    cJSON* root = cJSON_CreateObject(); // ARBOL PARA EL JSON
    if (root==NULL) return -1;

    struct Node* aux = l;

    while(aux != NULL) {
        cJSON* userTree = cJSON_CreateObject(); // ARBOL DE 1 USUSARIO

        // AÑADIR DATOS
        cJSON_AddNumberToObject(userTree, "conn", 0); // siempre desconectado
        cJSON_AddNumberToObject(userTree, "id", aux->id);
        //cJSON_AddStringToObject(userTree, "ip", (aux->ip[0]!='\0')?aux->ip : ""); No los almacenamos para evitar problemas de interrupción de señal
        //cJSON_AddNumberToObject(userTree, "port", aux->port);                     Damos por hecho que al terminar el servidor todos los usuarios se desconectan
        cJSON_AddNumberToObject(userTree, "num_pending", aux->num_pending);


        if (aux->pending != NULL && aux->num_pending > 0) {
            cJSON* msgTree = cJSON_CreateObject(); // ARBOL DE MENSAJES PENDIENTES
            for (int i=0; i<aux->num_pending; i++) {
                // CLAVE: "<sName>,<id>"
                char key[LENG*2 + 50];
                sprintf(key, "%s,%u,%s", aux->pending[i].sName, aux->pending[i].id, aux->pending[i].file);

                // AÑADIR DATOS
                cJSON_AddStringToObject(msgTree, key, aux->pending[i].msg);
            }
            cJSON_AddItemToObject(userTree, "pending", msgTree);
        } else cJSON_AddNullToObject(userTree, "pending");

        cJSON_AddItemToObject(root, aux->userName, userTree);

        aux = aux->next;
    }
    
    // Pasamos Objeto a Json
    char* json_string = cJSON_Print(root);

    FILE * fichero = fopen(name, "w");
    if(fichero == NULL) { cJSON_Delete(root); free(json_string); return -1;}

    fputs(json_string, fichero);
    fclose(fichero);

    cJSON_Delete(root);
    free(json_string);

    return 0;
}


int fromFile(List * l, char * name) {
    FILE * fichero = fopen(name, "r");
    if (fichero == NULL) return -1;

    destroy_list(l);

    fseek(fichero, 0, SEEK_END);
    long fsize = ftell(fichero);
    fseek(fichero, 0, SEEK_SET);

    if (fsize <= 0) { fclose(fichero); return 0;}   // base de datos vacía 

    char* json_string=malloc(fsize+1);
    if (json_string == NULL) {fclose(fichero); return -1;}
    
    fread(json_string, 1, fsize, fichero);
    json_string[fsize] = '\0';
    fclose(fichero);

    cJSON* root = cJSON_Parse(json_string);
    if (root == NULL) {free(json_string); return -1;}

    cJSON* userNode = NULL;
    cJSON_ArrayForEach(userNode, root) {
        char* uName = userNode->string;
        uint8_t conn = (uint8_t)cJSON_GetObjectItem(userNode, "conn")->valueint;
        if (setNode(l, uName, conn) != 0) goto error;

        struct Node* newNode = *l;
        newNode->id = (unsigned int)cJSON_GetObjectItem(userNode, "id")->valueint;

        // ip no está almacenada
        //cJSON* ipJson = cJSON_GetObjectItem(userNode, "ip");
        //if (cJSON_IsString(ipJson)) strncpy(newNode->ip, ipJson->valuestring, 16);

        // valor de port en setNode
        //newNode->port = (uint16_t)cJSON_GetObjectItem(userNode, "port")->valueint;

        cJSON* pendingJson = cJSON_GetObjectItem(userNode, "pending");
        newNode->num_pending = cJSON_GetObjectItem(userNode, "num_pending")->valueint;

        if (newNode->num_pending > 0 && cJSON_IsObject(pendingJson)) {
            newNode->pending=malloc(sizeof(struct msgdata)* newNode->num_pending);

            int i = 0;
            cJSON* msgNode = NULL;
            cJSON_ArrayForEach(msgNode, pendingJson) {
                if (i < newNode->num_pending) {
                    sscanf(msgNode->string, "%[^,],%u,%[^,]", newNode->pending[i].sName, &newNode->pending[i].id, newNode->pending[i].file);
                    strncpy(newNode->pending[i].msg, msgNode->valuestring, LENG);
                    i++;
                }
            }
        } else {newNode->pending=NULL; newNode->num_pending=0;}
    }

    cJSON_Delete(root);
    free(json_string);
    return 0;

error:
    cJSON_Delete(root);
    destroy_list(l);
    free(json_string);
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


int add_mssg_pending(List l, char* rName, char* sName, char* msg, unsigned int id, char * file) {
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

                        
            // copiar el nombre del fichero
            // arreglo bug segmentation
            if (file!=NULL){
                strncpy(nuevo->file, file, LENG);
                nuevo->file[LENG-1] = '\0';
            }
            else{
                nuevo->file[0]='\0';
            }

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

int get_users_conn_full(List l, int* n, char** users) {
    /* Funcion para obtener el numero y los nombres de usuarios conectados */
    if (l == NULL || n == NULL || users == NULL) return -1;

    int count = 0;
    struct Node *aux = l;

    // Recorremos la lista buscando usuarios con conn == 1
    while (aux != NULL) {
        if (aux->conn == 1) {
            // Copiamos el userName al array de punteros
            sprintf(users[count], "%s :: %s :: %d", aux->userName, aux->ip, aux->port);
            count++;
        }
        aux = aux->next;
    }

    // Guardamos el total en el puntero n
    *n = count;

    return 0; // Exito
}