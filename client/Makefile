.PHONY: all clean run debug

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -Iinclude -I../shared/include

# Eseguibile finale
TARGET = bin/client

# Sorgenti
SRC = src/main.c src/client.c src/utils.c
SHARED_SRC = ../shared/src/logging.c ../shared/src/protocol.c ../shared/src/game_logic.c

# File oggetto nella cartella obj/
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))
SHARED_OBJ = $(patsubst ../shared/src/%.c,obj/%.o,$(SHARED_SRC))

# Crea cartelle obj, bin e logs se non esistono
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
