# UNIVERSIDAD DEL VALLE DE GUATEMALA
# CC3069 - Computacion Paralela y Distribuida - Proyecto 02
# Compila la herramienta de archivos y las tres busquedas (seq y MPI).

CC      = gcc
MPICC   = mpicc
CFLAGS  = -std=c11 -O2 -Wall -Wextra
LDLIBS  = -lcrypto

SRC     = src
BIN     = bin

BINARIOS = $(BIN)/aes_archivo \
           $(BIN)/busqueda_secuencial \
           $(BIN)/busqueda_mpi

all: $(BINARIOS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/aes_archivo: $(SRC)/aes_archivo.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN)/busqueda_secuencial: $(SRC)/busqueda_secuencial.c $(SRC)/aes_comun.h $(SRC)/diccionario.h | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN)/busqueda_mpi: $(SRC)/busqueda_mpi.c $(SRC)/aes_comun.h $(SRC)/diccionario.h | $(BIN)
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -rf $(BIN)

.PHONY: all clean
