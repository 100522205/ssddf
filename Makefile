# Variables de compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I. -I/usr/include/tirpc
LDFLAGS = -lpthread -ltirpc

# para rpc

RPC_FILE= logger.x

# los stubs que deben borrarse y regenerarse
RPC_STUBS= logger_clnt.c logger_svc.c logger_xdr.c logger.h

# directorio para .o
OBJDIR = .o

# objetos para el servidor final
RPC_OBJS_SERV = $(OBJDIR)/logger_clnt.o $(OBJDIR)/logger_xdr.o 

# ejecutables
TARGET_SERV= server 
TARGET_RPC_LOG = logger_server 

# archivos fuente
COMMON_SRC= list.c sock.c cJSON.c 
SERV_SRC = servidor.c $(COMMON_SRC) 

# generar nombres de archivos .o
SERV_OBJS = $(addprefix $(OBJDIR)/, $(SERV_SRC:.c=.o)) $(RPC_OBJS_SERV)
RPC_LOGS_OBJS = $(OBJDIR)/logger_svc.o $(OBJDIR)/logger_xdr.o $(OBJDIR)/server-rpc.o 

# ppal

all: $(OBJDIR) rpc $(TARGET_SERV) $(TARGET_RPC_LOG)

# para el rcp

$(RPC_STUBS): $(RPC_FILE)
	rpcgen -NM $(RPC_FILE)

rpc: $(RPC_STUBS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# enlace del servidor
$(TARGET_SERV): $(SERV_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
$(TARGET_RPC_LOG): $(RPC_LOGS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# compilar .c a .o
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET_SERV) $(TARGET_RPC_LOG) $(RPC_STUBS)
