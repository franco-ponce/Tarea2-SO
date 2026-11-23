CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

SRCS = rw.cpp monitor_rw.cpp control_atomico_rw.cpp verificador.cpp
HDRS = comun.h monitor_rw.h control_atomico_rw.h verificador.h

rw: $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o rw

# Versión con ThreadSanitizer para buscar carreras de datos.
rw_tsan: $(SRCS) $(HDRS)
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=thread $(SRCS) -o rw_tsan

# Programa de prueba separado (con su propio main) para el verificador de traza.
prueba_verificador: prueba_verificador.cpp verificador.cpp verificador.h comun.h
	$(CXX) $(CXXFLAGS) prueba_verificador.cpp verificador.cpp -o prueba_verificador

pruebas: rw prueba_verificador
	./prueba_verificador
	bash pruebas_parametros.sh

# Experimentos (ver README.md). Escriben en resultados/.
e1: rw
	bash experimentos_e1.sh

e2: rw
	bash experimentos_e2.sh

fragmentos: rw
	bash fragmentos_traza.sh

clean:
	rm -f rw rw_tsan prueba_verificador

.PHONY: clean pruebas e1 e2 fragmentos
