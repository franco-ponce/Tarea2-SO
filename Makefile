CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

# Se agregan control_atomico_rw.cpp y verificador.cpp en las etapas siguientes.
SRCS = rw.cpp monitor_rw.cpp
HDRS = comun.h monitor_rw.h

rw: $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o rw

# Versión con ThreadSanitizer para buscar carreras de datos.
rw_tsan: $(SRCS) $(HDRS)
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=thread $(SRCS) -o rw_tsan

clean:
	rm -f rw rw_tsan

.PHONY: clean
