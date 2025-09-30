.PHONY: all server client clean clean-all run-server run-client install-deps test help

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

run-server:
	$(MAKE) -C server run

run-client:
	$(MAKE) -C client run

# Comandi utili per sviluppo
install-deps:
	# Qui potreste aggiungere eventuali dipendenze
	@echo "No dependencies to install"

test: all
	@echo "Running tests..."
	# Qui aggiungerete i test quando saranno pronti

help:
	@echo "Comandi disponibili:"
	@echo "  all         - Compila server e client"
	@echo "  server      - Compila solo il server"
	@echo "  client      - Compila solo il client"
	@echo "  clean       - Pulisce i file compilati"
	@echo "  clean-all   - Pulisce tutto inclusi i log"
	@echo "  run-server  - Compila ed esegue il server"
	@echo "  run-client  - Compila ed esegue il client"
	@echo "  test        - Esegue i test"
	@echo "  help        - Mostra questo messaggio"
