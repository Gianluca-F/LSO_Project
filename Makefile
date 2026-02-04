.PHONY: all server client clean clean-all run-server run-client valgrind-server valgrind-helgrind install-deps test help

all: server client

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean

clean-all: clean
	rm -rf server/logs/*
	rm -rf client/logs/*

run-server:
	$(MAKE) -C server run

run-client:
	$(MAKE) -C client run

# Esegui server con valgrind per memory leak detection
valgrind-server: server
	cd server && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes ./bin/server

# Esegui server con helgrind per race condition detection
valgrind-helgrind: server
	cd server && valgrind --tool=helgrind --history-level=full ./bin/server

# Comandi utili per sviluppo
install-deps:
	# Qui potreste aggiungere eventuali dipendenze
	@echo "No dependencies to install"

test: all
	@echo "Running tests..."
	# Qui aggiungerete i test quando saranno pronti

help:
	@echo "Comandi disponibili:"
	@echo "  all         		- Compila server e client"
	@echo "  server      		- Compila solo il server"
	@echo "  client      		- Compila solo il client"
	@echo "  clean       		- Pulisce i file compilati"
	@echo "  clean-all   		- Pulisce tutto inclusi i log"
	@echo "  run-server  		- Compila ed esegue il server"
	@echo "  run-client  		- Compila ed esegue il client"
	@echo "  valgrind-server 	- Esegue il server con Valgrind per memory leak detection"
	@echo "  valgrind-helgrind	- Esegue il server con Helgrind per race condition detection"
	@echo "  install-deps 		- Installa eventuali dipendenze necessarie"
	@echo "  test       		- Esegue i test"
	@echo "  help       		- Mostra questo messaggio"