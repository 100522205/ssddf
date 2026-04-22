# Variables de compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -lpthread

# Directorio para archivos objeto
OBJDIR = .o

# Ejecutables finales
TARGET_SERV = server

# Archivos fuente
COMMON_SRC = list.c sock.c
SERV_SRC = servidor.c $(COMMON_SRC)

# Generar nombres de archivos .o dentro de $(OBJDIR)
SERV_OBJS = $(addprefix $(OBJDIR)/, $(SERV_SRC:.c=.o))
CLI_OBJS = $(addprefix $(OBJDIR)/, $(CLI_SRC:.c=.o))

# Regla principal
all: $(OBJDIR) $(TARGET_SERV) $(TARGET_CLI)

# Crear el directorio .o si no existe
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Enlace del servidor
$(TARGET_SERV): $(SERV_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Regla genérica para compilar archivos .c a .o en la carpeta $(OBJDIR)
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -rf $(OBJDIR) $(TARGET_SERV) $(TARGET_CLI)

.PHONY: all clean