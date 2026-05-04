from enum import Enum
import argparse
import socket
import sys
import threading

# para el web service
import requests

# método de client para requests de web service
def preprocesado(texto : str):
    """
    Función genérica para preprocesado
    :param texto: el texto pudiendo tener mal los espacios
    :return: el texto ya bien 
    """

    try:
        r = requests.post(url="http://127.0.0.1:7777/normalizar",
                        json={ "texto": texto },
                        headers={ 'Content-type': 'application/json' })
        
        # en r nosotros tenemos la respuesta del servicio web a nuestro post

        if(r.status_code==200):
            # fue correcto 
            respuesta=r.json()

            texto_limpio=respuesta["texto"]
            return texto_limpio, 0
        else:
            print("\n ERROR EN WEB SERVICE \n")
            return texto, -1
    
    except Exception as e:
        print("ERROR INESPERADO EN WEB SERVICE", e)
        return texto, -1




class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    # IP y Puerto DEL SERVIDOR
    _server = None
    _port = -1
    _thread_port = -1
    _thread = -1
    _continue = 1
    _me = ""
    _conn = ""

    # ******************** METHODS *******************
    # *
    # * @param user - User name to register in the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        #  Función para registrar un usuario en el sistema
        
        if(type(user)!=str):
            print("Error en connect: Introduce el user como un string\n")
            return client.RC.ERROR
        
        # primero, crear el socket
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            my_address= ('localhost', 0)
            sock.bind(my_address)
        except Exception as e:
            print("Error en register: Fallo en creación de socket"+str(e))
            return client.RC.ERROR
        
        # ahora, conectar al servidor
        try:
            server_address= (client._server, client._port)
            sock.connect(server_address)
        except Exception as e:
            print("Error en register: Fallo en conexión con servidor"+str(e))
            sock.close()
            return client.RC.ERROR
        
        # ahora, enviar REGISTER y el nombre
        try:
            messages=["REGISTER", user]
            for m in messages:
                sock.sendall((m+"\0").encode('utf-8'))
        
        except Exception as e:
            print("Error en register: Fallo en envio de cadenas"+str(e))
            sock.close()
            return client.RC.ERROR
        
        # ahora, recibir el byte de resultado

        try:
            msg=sock.recv(1)
            msg=int.from_bytes(msg, byteorder='big')
            if msg==0:
                print("REGISTER OK\n")
            elif msg==1:
                print("USERNAME IN USE\n")
                return client.RC.USER_ERROR
            else:
                print("REGISTER FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("Error en register: Fallo en recepción"+str(e))
            sock.close()
            return client.RC.ERROR
        
        if msg==0:
            client._me=user
        sock.close()
        return client.RC.OK

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 * 
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
        #  Función para desregistrar un usuario en el sistema
        
        if(type(user)!=str or user!=client._me):
            print("Error en unregister: Introduce el user como un string una vez estés registrado o conectado\n")
            return client.RC.ERROR
        
        # primero, crear el socket
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            my_address= ('localhost', 0)
            sock.bind(my_address)
        except Exception as e:
            print("Error en unregister: Fallo en creación de socket"+str(e))
            return client.RC.ERROR
        
        # ahora, conectar al servidor
        try:
            server_address= (client._server, client._port)
            sock.connect(server_address)
        except Exception as e:
            sock.close()
            print("Error en unregister: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR
        
        # ahora, enviar REGISTER y el nombre
        try:
            messages=["UNREGISTER", user]
            for m in messages:
                sock.sendall((m+"\0").encode('utf-8'))
        
        except Exception as e:
            sock.close()
            print("Error en unregister: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR
        
        # ahora, recibir el byte de resultado

        try:
            msg=sock.recv(1)
            msg=int.from_bytes(msg, byteorder='big')
            if msg==0:
                print("UNREGISTER OK\n")
                client._me=""
            elif msg==1:
                print("USER DOES NOT EXIST\n")
                return client.RC.USER_ERROR
            else:
                print("UNREGISTER FAIL\n")
                return client.RC.ERROR
        except Exception as e:
            sock.close()
            print("Error en unregister: Fallo en recepción"+str(e))
            return client.RC.ERROR
        sock.close()
        return client.RC.OK


    # *
    # * @param user - User name to connect to the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        #  Funcion para conectarse al servidor

        # testeo de parámetros

        if(type(user)!=str):
            print("Error en connect: Introduce el user como un string\n")
            return client.RC.ERROR

        # primero: creacion de sockets con adress
        try:
            # comunicación genérica
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock.bind(address)

            #para el worker
            sock_thread = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address_thread = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock_thread.bind(address_thread)
            sock_thread.listen(10) # para recepcion de mas de 1


        except Exception as e:
            print("Error en connect: Fallo en creación de socket"+str(e))
            return client.RC.ERROR

        # ahora, a la conexión

        try:
            address_servidor = (client._server, client._port)
            sock.connect(address_servidor) # conecta SOLO EL QUE ENVIA, por eso el thread no

        except Exception as e:
            print("Error en connect: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR

        # creacion del thread
        client._continue=1
        client._thread= threading.Thread(target=client.worker, args=(sock_thread,), daemon=True)
        client._thread.start()
        # ahora toca enviar dos cadenas, CONNECT y el nombre, y luego el puerto

        try:
            mensajes = ["CONNECT", user, str(sock_thread.getsockname()[1])]
            for m in mensajes:
                sock.sendall((m+"\0").encode('utf-8'))

        except Exception as e:
            sock.close()
            print("Error en connect: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR

        # ahora toca recibir el byte de resultado

        try:
            msg=sock.recv(1)
            msg=int.from_bytes(msg, byteorder='big')
            
            if msg!=2:
                print("CONNECT", end = " ")
            if 0== msg:
                print("OK\n")
            elif 1==msg:
                print("FAIL, USER DOES NOT EXIST\n")
                return client.RC.USER_ERROR
            elif 2==msg:
                print("USER ALREADY CONNECTED\n")
                return client.RC.USER_ERROR
            elif 3== msg:
                print("CONNECT FAIL\n")
                return client.RC.ERROR
            else:
                print("UNEXPECTED!!!!\n")
                return client.RC.ERROR
            
        except Exception as e:
            sock.close()
            print("Error en connect: Fallo en recepción"+str(e))
            return client.RC.ERROR
        
        sock.close()
        client._me=user
        client._conn=user
        return client.RC.OK

    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        # Funcion para conocer los usuarios conectados

        # primero: creacion de socket
        try:
            # comunicación genérica
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock.bind(address)


        except Exception as e:
            print("Error en connect: Fallo en creación de socket"+str(e))
            return client.RC.ERROR

        # ahora, a la conexión

        try:
            address_servidor = (client._server, client._port)
            sock.connect(address_servidor)

        except Exception as e:
            print("Error en connect: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR

        try:
            mensajes = ["USERS", client._me]
            for m in mensajes:
                sock.sendall((m+"\0").encode('utf-8'))

        except Exception as e:
            sock.close()
            print("Error en connect: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR

        # ahora toca recibir el byte de resultado

        try:
            msg=sock.recv(1)
            msg=int.from_bytes(msg, byteorder='big')
            
            print("CONNECTED USERS", end = " ")
            if 0== msg:
                n_user_bytes = sock.recv(4)
                n_user = int.from_bytes(n_user_bytes, byteorder='big')
                print("("+ str(n_user)+" users connected) OK")
                for i in range(n_user):
                    name = sock.recv(255).decode('utf-8').rstrip('\x00')
                    print(name)
            elif 1==msg:
                print("FAIL, USER IS NOT CONNECTED\n")
                return client.RC.USER_ERROR
            elif 2==msg:
                print("FAIL\n")
                return client.RC.ERROR
            else:
                print("UNEXPECTED!!!!\n")
                return client.RC.ERROR
            
        except Exception as e:
            sock.close()
            print("Error en connect: Fallo en recepción"+str(e))
            return client.RC.ERROR
        
        sock.close()
        return client.RC.OK



    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        #  Funcion para desconectarse al servidor

        # testeo de parámetros

        if(type(user)!=str or user != client._me):
            print("Error en disconnect: Introduce el user como un string y asegurate de estar conectado\n")
            return client.RC.ERROR
        
        # antes de nada, matar el thread

        client._continue=0

        # ahora: creacion de sockets con adress
        try:
            # comunicación genérica
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock.bind(address)

        except Exception as e:
            print("Error en disconnect: Fallo en creación de socket"+str(e))
            return client.RC.ERROR

        # ahora, a la conexión

        try:
            address_servidor = (client._server, client._port)
            sock.connect(address_servidor)

        except Exception as e:
            print("Error en disconnect: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR

        # ahora toca enviar dos cadenas, CONNECT y el nombre

        try:
            mensajes = ["DISCONNECT", user]
            for m in mensajes:
                sock.sendall((m+"\0").encode('utf-8'))

        except Exception as e:
            sock.close()
            print("Error en disconnect: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR

        # ahora toca recibir el byte de resultado

        try:
            msg=sock.recv(1)
            msg=int.from_bytes(msg, 'big')
            print("DISCONNECT ", end = "")
            if 0== msg:
                print("OK\n")
                client._me=""
            elif 1== msg:
                print("FAIL, USER DOES NOT EXIST\n")
                return client.RC.USER_ERROR
            elif 2== msg:
                print("FAIL, USER NOT CONNECTED\n")
                return client.RC.USER_ERROR
            elif 3== msg:
                print("FAIL\n")
                return client.RC.ERROR
            else:
                print("UNEXPECTED!!!!\n")
                return client.RC.ERROR
            
        except Exception as e:
            sock.close()
            print("Error en disconnect: Fallo en recepción"+str(e))
            return client.RC.ERROR
        
        sock.close()
        client._conn=""
        return client.RC.OK

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        #  Funcion para enviar un mensaje

        # testeo de parámetros

        if(client._conn=="" or client._me==user or type(user)!=str or type(message)!=str or len(message)>255):
            print("Error en send: Introduce parámetros correctos o realiza operación de conexión\n")
            return client.RC.ERROR
        
        # corrección del mensaje por el web service

        message = preprocesado(message)
        if(message[1]<0):
            print("Error en send: Error en preprocesado de texto\n")
            return client.RC.ERROR

        message= message[0]

        # ahora: creacion de sockets con adress
        try:
            # comunicación genérica
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock.bind(address)

        except Exception as e:
            print("Error en send: Fallo en creación de socket"+str(e))
            return client.RC.ERROR

        # ahora, a la conexión

        try:
            address_servidor = (client._server, client._port)
            sock.connect(address_servidor)

        except Exception as e:
            print("Error en send: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR

        # ahora toca enviar dos cadenas, CONNECT y el nombre

        try:
            mensajes = ["SEND", client._me, user, message]
            for m in mensajes:
                sock.sendall((m+"\0").encode('utf-8'))

        except Exception as e:
            sock.close()
            print("Error en send: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR

        # ahora toca recibir el byte de resultado

        try:
            msg= int.from_bytes(sock.recv(1), byteorder='big')
            
            if 0==msg:
                
                id_mensaje = client.read_string(sock)
                print("SEND OK - MESSAGE " + str(id_mensaje))
            elif 1==msg:
                print("SEND FAIL, USER DOES NOT EXIST\n")
                return client.RC.USER_ERROR
            elif 2==msg:
                print("SEND FAIL\n")
                return client.RC.ERROR
            else:
                print("UNEXPECTED!!!!\n")
                return client.RC.ERROR
            
        except Exception as e:
            sock.close()
            print("Error en send: Fallo en recepción"+str(e))
            return client.RC.ERROR
        
        sock.close()
        return client.RC.OK
    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user,  file,  message) :
        """
        funcion para enviar un fichero a un usuario
        :param file: ASUMO que es el nombre
        """


        # testeo de parámetros

        if(client._conn=="" or client._me==user or type(user)!=str or type(message)!=str or len(message)>255 or type(file)!=str or file=="" or len(file)>255):
            print("Error en sendattach: Introduce parámetros correctos o realiza operación de conexión\n")
            return client.RC.ERROR
        
        # corrección del mensaje por el web service

        message = preprocesado(message)
        if(message[1]<0):
            print("Error en sendattach: Error en preprocesado de texto\n")
            return client.RC.ERROR

        message= message[0]

        # ahora: creacion de sockets con adress
        try:
            # comunicación genérica
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            address = ('localhost', 0) # al darle 0, el SO nos da uno libre
            sock.bind(address)

        except Exception as e:
            print("Error en sendattach: Fallo en creación de socket"+str(e))
            return client.RC.ERROR

        # ahora, a la conexión

        try:
            address_servidor = (client._server, client._port)
            sock.connect(address_servidor)

        except Exception as e:
            print("Error en sendattach: Fallo en conexión con servidor"+str(e))
            return client.RC.ERROR

        # ahora toca enviar dos cadenas, CONNECT y el nombre

        try:
            mensajes = ["SENDATTACH", client._me, user, message, file]
            for m in mensajes:
                sock.sendall((m+"\0").encode('utf-8'))

        except Exception as e:
            sock.close()
            print("Error en sendattach: Fallo en envio de cadenas"+str(e))
            return client.RC.ERROR

        # ahora toca recibir el byte de resultado

        try:
            msg= int.from_bytes(sock.recv(1), byteorder='big')
            
            if 0==msg:
                # sabemos que fue bien, podemos recibir el ID que le ha asignado el servidor 
                id= client.read_string(sock)
                print("SENDATTACH OK - MESSAGE " + str(id))
            elif 1==msg:
                print("SENDATTACH FAIL, USER DOES NOT EXIST\n")
                return client.RC.USER_ERROR
            elif 2==msg:
                print("SENDATTACH FAIL\n")
                return client.RC.ERROR
            else:
                print("UNEXPECTED!!!!\n")
                return client.RC.ERROR
            
        except Exception as e:
            sock.close()
            print("Error en sendattach: Fallo en recepción"+str(e))
            return client.RC.ERROR
        
        sock.close()
        return client.RC.OK
    
    # *
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message

    @staticmethod
    def  worker(socket_th) :
        #  Write your code here
        while(client._continue):
            socket, address=socket_th.accept()

            operacion = client.read_string(socket)
            if(operacion=="SEND_MESSAGE"):
                remitente=client.read_string(socket)
                my_id=client.read_string(socket)
                contenido=client.read_string(socket)

                print("MESSAGE "+str(my_id)+ " FROM "+str(remitente) + "\n"+str(contenido)+ "\nEND")
                
                print("c> ", end="", flush=True)

            elif operacion=="SEND_MESS_ACK":
                msg_id = client.read_string(socket)
                # no imprimimos nada porque el enunciado no lo dice
            elif operacion== "SEND_MESSAGE_ATTACH":
                remitente=client.read_string(socket)
                my_id=client.read_string(socket)
                contenido=client.read_string(socket)
                fichero=client.read_string(socket)
                print(f"\nFILE {fichero} FROM {remitente} (ID {my_id}): {contenido}\n") 
                # lo dice asi el enunciado (?) mirar
            
            socket.close()

        socket_th.close()
        return client.RC.OK

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            #  Remove first two words
                            message = ' '.join(line[3:])
                            client.sendAttach(line[1], line[2], message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            if (client._conn!=""):
                                client.disconnect(client._conn)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief leer string entero
    @staticmethod
    def read_string(sock):
        # para poder leer el string que llega desde el servidor
        
        string = b""
        
        while(1):
            # leemos poco a pco
            local = sock.recv(1)
            if(not local or local==b'\0'):
                break
            string+=local
        return string.decode('utf-8')
    
    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])
