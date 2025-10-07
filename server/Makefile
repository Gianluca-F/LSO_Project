.PHONY: all clean run debug

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -Iinclude -I../shared/include

# Eseguibile finale
TARGET = bin/server

# Sorgenti
SRC = src/main.c src/server.c src/utils.c
SHARED_SRC = ../shared/src/protocol.c ../shared/src/game_logic.c

# File oggetto nella cartella obj/
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))
SHARED_OBJ = $(patsubst ../shared/src/%.c,obj/%.o,$(SHARED_SRC))

# Crea cartelle obj e bin se non esistono
DIRS = obj bin logs

all: $(DIRS) $(TARGET)

$(DIRS):
	mkdir -p $@

# Regola per creare il binario finale
$(TARGET): $(OBJ) $(SHARED_OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $^

# Regola per creare i file .o
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# Regola per i file shared
obj/%.o: ../shared/src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# Pulizia
clean:
	rm -rf $(DIRS:%=%/*)

# Esegue il programma
run: $(TARGET)
	./$(TARGET)

# Debug
debug: CFLAGS += -DDEBUG
debug: $(TARGET)

# Comandi utili per lo sviluppo
help:
	@echo "Comandi disponibili:"
	@echo "  make          - Compila il progetto"
	@echo "  make clean    - Pulisce i file compilati"
	@echo "  make run      - Esegue il server"
	@echo "  make debug    - Compila con opzioni di debug"
	@echo "  make help     - Mostra questo messaggio di aiuto"
