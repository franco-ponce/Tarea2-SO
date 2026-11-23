// prueba_verificador.cpp - Ejecutable de prueba separado: comprueba que verificar_traza
// acepta trazas válidas y detecta cada tipo de violación, usando trazas construidas a mano.

#include "verificador.h"

#include <cstdio>
#include <string>
#include <vector>

thread_local int id_hebra = -1;
thread_local ContadoresFallos fallos_hebra;

static const Tipo L = Tipo::Lectora;
static const Tipo W = Tipo::Escritora;
static const Accion P = Accion::Pide;
static const Accion N = Accion::Entra;
static const Accion S = Accion::Sale;

static int fallos = 0;

static void caso(const char* nombre, Politica pol, const std::vector<AnotacionTraza>& t, bool ok_esperado,
                 long evidencia_esperada = -1, const char* regla_esperada = nullptr) {
    ResultadoVerificacion r = verificar_traza(t, pol);
    bool bien = r.politica_ok == ok_esperado;
    if (evidencia_esperada >= 0 && r.evidencia != evidencia_esperada) bien = false;
    if (regla_esperada != nullptr &&
        (r.violaciones.empty() || r.violaciones[0].regla.find(regla_esperada) == std::string::npos))
        bien = false;
    std::printf("%s  %-62s politica_ok=%s evidencia=%ld", bien ? "ok   " : "FALLO", nombre,
                r.politica_ok ? "SI" : "NO", r.evidencia);
    if (!r.violaciones.empty())
        std::printf("  [anotacion %zu: %s]", r.violaciones[0].indice, r.violaciones[0].regla.c_str());
    std::printf("\n");
    if (!bien) ++fallos;
}

int main() {
    const Politica LEC = Politica::Lectores, ESC = Politica::Escritores, FIFO = Politica::Fifo;

    // --- Reglas comunes ---
    caso("todas: escritora entra con lectora activa", ESC,
         {{0, L, P, -1}, {0, L, N, -1}, {1, W, P, -1}, {1, W, N, -1}, {1, W, S, -1}, {0, L, S, -1}},
         false, -1, "Todas");
    caso("todas: lectora entra con escritora activa", LEC,
         {{1, W, P, -1}, {1, W, N, -1}, {0, L, P, -1}, {0, L, N, -1}, {0, L, S, -1}, {1, W, S, -1}},
         false, -1, "Todas");
    caso("todas: dos escritoras a la vez", FIFO,
         {{1, W, P, 0}, {1, W, N, 0}, {2, W, P, 1}, {2, W, N, 1}, {1, W, S, 0}, {2, W, S, 1}},
         false, -1, "Todas");

    // --- Lectores ---
    caso("lectores: válida, lectora entra con escritora pendiente", LEC,
         {{0, L, P, -1}, {0, L, N, -1}, {2, W, P, -1}, {1, L, P, -1}, {1, L, N, -1},
          {0, L, S, -1}, {1, L, S, -1}, {2, W, N, -1}, {2, W, S, -1}},
         true, 1);
    caso("lectores: escritora entra con lectora pendiente", LEC,
         {{2, W, P, -1}, {2, W, N, -1}, {0, L, P, -1}, {2, W, S, -1}, {3, W, P, -1}, {3, W, N, -1},
          {3, W, S, -1}, {0, L, N, -1}, {0, L, S, -1}},
         false, -1, "Lectores: escritora");
    caso("lectores: lectora pide sin escritora activa y no entra enseguida", LEC,
         {{0, L, P, -1}, {2, W, P, -1}, {0, L, N, -1}, {0, L, S, -1}, {2, W, N, -1}, {2, W, S, -1}},
         false, -1, "Lectores: lectora pide");

    // --- Escritores ---
    caso("escritores: válida, lectora pide con escritora pendiente", ESC,
         {{0, L, P, -1}, {0, L, N, -1}, {2, W, P, -1}, {1, L, P, -1}, {0, L, S, -1},
          {2, W, N, -1}, {2, W, S, -1}, {1, L, N, -1}, {1, L, S, -1}},
         true, 1);
    caso("escritores: lectora entra con escritora pendiente", ESC,
         {{0, L, P, -1}, {0, L, N, -1}, {2, W, P, -1}, {1, L, P, -1}, {1, L, N, -1},
          {0, L, S, -1}, {1, L, S, -1}, {2, W, N, -1}, {2, W, S, -1}},
         false, -1, "Escritores");

    // --- FIFO ---
    caso("fifo: válida, lectoras consecutivas concurrentes", FIFO,
         {{0, L, P, 0}, {0, L, N, 0}, {1, L, P, 1}, {1, L, N, 1}, {2, W, P, 2}, {3, L, P, 3},
          {0, L, S, 0}, {1, L, S, 1}, {2, W, N, 2}, {2, W, S, 2}, {3, L, N, 3}, {3, L, S, 3}},
         true, 1);
    caso("fifo: lectora se adelanta a escritora con turno anterior", FIFO,
         {{0, L, P, 0}, {0, L, N, 0}, {2, W, P, 1}, {3, L, P, 2}, {3, L, N, 2}, {0, L, S, 0},
          {3, L, S, 2}, {2, W, N, 1}, {2, W, S, 1}},
         false, -1, "FIFO");
    caso("fifo: turnos de Pide no consecutivos", FIFO,
         {{0, L, P, 0}, {0, L, N, 0}, {0, L, S, 0}, {1, L, P, 5}, {1, L, N, 5}, {1, L, S, 5}},
         false, -1, "FIFO: los turnos");

    // --- Coherencia ---
    caso("coherencia: Entra sin Pide", ESC, {{0, L, N, -1}, {0, L, S, -1}}, false, -1, "coherencia");
    caso("coherencia: traza termina con lectora activa", ESC, {{0, L, P, -1}, {0, L, N, -1}}, false, -1,
         "coherencia");
    caso("traza vacía", FIFO, {}, true, 0);

    std::printf("casos con fallo: %d\n", fallos);
    return fallos == 0 ? 0 : 1;
}
