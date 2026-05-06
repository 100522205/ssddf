"""
Jose Barrio Y Juan Mayoral
Archivo que contiene TODOS los tests de nuestro programa
Ejecución: python3 ./test.py

Que se prueba: que se siga el protocolo 
"""

import socket
import subprocess 
import time
import os
import struct

# conf de tests

IP_S = "127.0.0.1"
IP_RPC = "127.0.0.1"
PUERTO_S = 8888

# funciones aux para los tests con strings
# como las del cliente

def send_string(sock,s):
    sock.sendall(str(s).encode("utf-8")+ b"\0")

def recv_byte(sock):
    data = sock.recv(1)
    return data[0] if data else -1

def recv_str(sock):
    buf = bytearray()
    while 1:
        char = sock.recv(1)
        if not char or char== b"\0":
            break
        buf.extend(char)
    
    return buf.decode("utf-8")

# El cliente : Vamos a simularlo

class myClient:
    """
    clase del cliente simulado
    """

    def __init__(self, name, port):
        self.name = name
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # queremos: ESCUCHAR EL THREAD CONC DEL CLIENTE
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", port))
        self.sock.listen(1)

        self.sock.settimeout(2.0)

    def req(self, *args):
        """
        Funcion para enviar argumentos separados por \0 y darme la respuesta
        """

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2.0)
            s.connect((IP_S, PUERTO_S))
            for a in args:
                # lo enviamos
                send_string(s, a)
            
            res=recv_byte(s)

            # NOTA: Y con SEND o SENDATTACH? tenemos ID !!!!
            # vamos a intentar capturarlo si es uno de estos

            e = recv_str(s) if res==0 and args[0] in ("SEND", "SENDATTACH") else None

            return res, e
        
    def accept_ack(self):
        try:
            conn, _ = self.sock.accept()
            conn.clse()
        except:
            pass


def is_same(test, actual, exp):
    """
    Funcion para ver si fue correcto
    """

    if(actual==exp):
        print("Test ", str(test) , "pasado")
        return 1
    
    print("Test ", str(test) , "SUSPENSO")
    print("Se obtuvo",str(actual), "cuando se esperaba", str(exp))
    return 0

"""
Ahora : LOS TESTS
"""

def run():
    """
    Función para la ejecución de TODOS los tests
    """
    print("Ejecución de Tests Laboratorio Final")
    print("Jose Barrio, Juan Mayoral")

    # dos clientes
    c1 = myClient("Juan", 9001)
    c2 = myClient("Jose", 9002)

    tot=9
    pasados=0

    try:
        # Test 0 y 1: Registrar bien y repetido
        pasados += is_same(0, c1.req("REGISTER", c1.name)[0], 0)
        pasados += is_same(1, c1.req("REGISTER", c1.name)[0], 1)
        c2.req("REGISTER", c2.name)

        # test 2 : conectarse
        pasados += is_same(2, c1.req("CONNECT", c1.name, c1.port)[0], 0)

        # test 3: lista de usuarios
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((IP_S, PUERTO_S))
            send_string(s, "USERS")
            send_string(s, c1.name)

            res= recv_byte(s)

            # conv

            n_users=struct.unpack("!I", s.recv(4))[0] if res==0 else 0
                
        pasados += is_same(3, (res, n_users), (0,1))

        # test 4: envio que no llega: no conectado

        res, id = c1.req("SEND", c1.name, c2.name, "PRUEBA")
        c1.accept_ack()
        pasados += is_same(4, res, 0)

        # test 5: forwarding

        c2.req("CONNECT", c2.name, c2.port)

        # el server pide al socket
        conn, id = c2.sock.accept()

        # obtener los strings
        op= recv_str(conn)
        emis= recv_str(conn)
        id = recv_str(conn)
        msg=recv_str(conn)
        conn.close()
        
        c1.accept_ack()


        pasados += is_same(5, (op, emis, msg), ("SEND_MESSAGE", c1.name, "PRUEBA"))

        # test 6: envio de attach
        res, att_id = c1.req("SENDATTACH", c1.name, c2.name, "FICHERO", "datos.txt")
        c1.accept_ack()

        pasados += is_same(6, res, 0)

        # test 7: Recibir el mensaje que iba con attach
        conn, id = c2.sock.accept()
        
        # otra vez, los obtenemos 1 a 1
        op=recv_str(conn)
        sender = recv_str(conn)
        id =recv_str(conn)
        msg=recv_str(conn)
        file=recv_str(conn)

        conn.close()

        pasados += is_same(7, (op, msg, file), ("SEND_MESSAGE_ATTACH", "FICHERO", "datos.txt"))

        # test 8: desconectar

        pasados += is_same(8, c1.req("DISCONNECT", c1.name)[0], 0)

    except Exception as e:
        print("ERROR: "+ str(e))

    print("Total obtenido: ",pasados, " de ", tot)

"""
Ahora, queremos que esto ejeute TODO, o hacemos por subprocess
"""

if __name__ == "__main__":
    print("Paso 0: Compilando a Make")
    subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["make", "all"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("Paso 1: Ejecutar los procesos")

    # Levantar el webService

    web= subprocess.Popen(["python3", "webService.py"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 

    # levantamos el RPC
    rpc = subprocess.Popen(["./logger_server"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 
    time.sleep(1)

    # levantamos el server
    env=os.environ.copy()
    env["LOG_RPC_IP"]= IP_RPC
    srv = subprocess.Popen(["./server", "-p", str(PUERTO_S)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    print("Paso 2: Ejecutar tests")

    
    run()

    print("Paso 3: Limpiando procesos")
    srv.kill()
    rpc.kill()
    web.kill()
    




