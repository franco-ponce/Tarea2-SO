// rw.cpp - Programa principal: parámetros, hebras, registro compartido, invariantes y medición.
//
// Uso: ./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]

#include "comun.h"
#include "monitor_rw.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

thread_local int id_hebra = -1;
thread_local ContadoresFallos fallos_hebra;

const char* nombre_politica(Politica p) {
    switch (p) {
        case Politica::Lectores:   return "lectores";
        case Politica::Escritores: return "escritores";
        case Politica::Fifo:       return "fifo";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Parámetros (R1)
// ---------------------------------------------------------------------------

enum class Modo { Cond, Atomic };

struct Parametros {
    Modo modo;
    Politica politica;
    long L, E, K;
    long dur_lec_us, dur_esc_us;
    bool con_traza = true;
};

static void uso(const char* prog) {
    std::fprintf(stderr,
        "Uso: %s <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]\n"
        "  modo:     cond | atomic\n"
        "  politica: lectores | escritores | fifo   (atomic solo admite escritores)\n"
        "  L >= 1, E >= 1, L + E <= 1000, K >= 1, duraciones >= 0 (microsegundos)\n",
        prog);
}

// Convierte un entero decimal completo; falla si hay basura, signo inválido o desborde.
static bool leer_long(const char* s, long& out) {
    if (s == nullptr || *s == '\0') return false;
    errno = 0;
    char* fin = nullptr;
    long v = std::strtol(s, &fin, 10);
    if (errno == ERANGE || fin == s || *fin != '\0') return false;
    out = v;
    return true;
}

static bool leer_parametros(int argc, char** argv, Parametros& p) {
    if (argc != 8 && argc != 9) {
        std::fprintf(stderr, "Error: número de argumentos incorrecto.\n");
        return false;
    }

    std::string modo = argv[1];
    if (modo == "cond") p.modo = Modo::Cond;
    else if (modo == "atomic") p.modo = Modo::Atomic;
    else { std::fprintf(stderr, "Error: modo desconocido '%s'.\n", argv[1]); return false; }

    std::string pol = argv[2];
    if (pol == "lectores") p.politica = Politica::Lectores;
    else if (pol == "escritores") p.politica = Politica::Escritores;
    else if (pol == "fifo") p.politica = Politica::Fifo;
    else { std::fprintf(stderr, "Error: política desconocida '%s'.\n", argv[2]); return false; }

    if (p.politica == Politica::Fifo) {  // provisorio, hasta la Etapa 3
        std::fprintf(stderr, "Error: la política fifo aún no está implementada.\n");
        return false;
    }

    if (p.modo == Modo::Atomic && p.politica != Politica::Escritores) {
        std::fprintf(stderr, "Error: el modo atomic solo implementa la política escritores.\n");
        return false;
    }

    const char* nombres[] = {"L", "E", "K", "dur_lec_us", "dur_esc_us"};
    long* destinos[] = {&p.L, &p.E, &p.K, &p.dur_lec_us, &p.dur_esc_us};
    for (int i = 0; i < 5; ++i) {
        if (!leer_long(argv[3 + i], *destinos[i])) {
            std::fprintf(stderr, "Error: %s='%s' no es un entero válido.\n", nombres[i], argv[3 + i]);
            return false;
        }
    }

    if (p.L < 1) { std::fprintf(stderr, "Error: L debe ser >= 1.\n"); return false; }
    if (p.E < 1) { std::fprintf(stderr, "Error: E debe ser >= 1.\n"); return false; }
    if (p.L + p.E > 1000) { std::fprintf(stderr, "Error: L + E debe ser <= 1000.\n"); return false; }
    if (p.K < 1) { std::fprintf(stderr, "Error: K debe ser >= 1.\n"); return false; }
    if (p.dur_lec_us < 0 || p.dur_esc_us < 0) {
        std::fprintf(stderr, "Error: las duraciones deben ser >= 0.\n");
        return false;
    }

    // 2EK y (L+E)K deben caber en un long.
    long ek, dos_ek, total;
    if (__builtin_mul_overflow(p.E, p.K, &ek) || __builtin_mul_overflow(ek, 2L, &dos_ek) ||
        __builtin_mul_overflow(p.L + p.E, p.K, &total)) {
        std::fprintf(stderr, "Error: 2EK o (L+E)K no caben en un long.\n");
        return false;
    }

    if (argc == 9) {
        if (std::strcmp(argv[8], "notraza") != 0) {
            std::fprintf(stderr, "Error: argumento opcional desconocido '%s' (se espera notraza).\n", argv[8]);
            return false;
        }
        p.con_traza = false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Control provisorio: exclusión total con mutex + variable de condición. Lo usa
// el modo atomic hasta que exista ControlAtomicoRW (Etapa 5).
// ---------------------------------------------------------------------------

class ControlProvisorio : public ControlRW {
public:
    void entrar_lectura() override { entrar(); }
    void salir_lectura() override { salir(); }
    void entrar_escritura() override { entrar(); }
    void salir_escritura() override { salir(); }
private:
    void entrar() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return !ocupado; });
        ocupado = true;
    }
    void salir() {
        std::unique_lock<std::mutex> lk(m);
        ocupado = false;
        cv.notify_one();
    }
    std::mutex m;
    std::condition_variable cv;
    bool ocupado = false;
};

// Crea el control según el modo. Si es un monitor, deja además un puntero a él en
// 'monitor' para poder leer la traza al final.
static std::unique_ptr<ControlRW> crear_control(const Parametros& p, MonitorRW*& monitor) {
    monitor = nullptr;
    if (p.modo == Modo::Cond) {
        auto mon = std::make_unique<MonitorRW>(p.politica, p.con_traza);
        monitor = mon.get();
        return mon;
    }
    return std::make_unique<ControlProvisorio>();
}

// ---------------------------------------------------------------------------
// Diagnóstico (R2): contadores atómicos separados del estado de admisión.
// ---------------------------------------------------------------------------

struct Diagnostico {
    std::atomic<long> lectoras_dentro{0};
    std::atomic<long> escritoras_dentro{0};
    std::atomic<long> max_lectoras{0};
    std::atomic<long> violaciones_exclusion{0};
    std::atomic<long> inconsistencias{0};
};

// Resultado que cada hebra llena localmente y que se suma después de los join().
struct ResultadoHebra {
    long operaciones = 0;
    double espera_total_us = 0;
    double espera_max_us = 0;
    ContadoresFallos fallos;
};

using Reloj = std::chrono::steady_clock;

static double micros(Reloj::duration d) {
    return std::chrono::duration<double, std::micro>(d).count();
}

static void actualizar_maximo(std::atomic<long>& maximo, long valor) {
    long actual = maximo.load();
    while (valor > actual && !maximo.compare_exchange_weak(actual, valor)) {
    }
}

static void hebra_lectora(int id, ControlRW& ctl, Registro& reg, Diagnostico& diag,
                          long K, long dur_us, ResultadoHebra& res) {
    id_hebra = id;
    fallos_hebra = ContadoresFallos{};
    for (long i = 0; i < K; ++i) {
        auto t0 = Reloj::now();
        ctl.entrar_lectura();
        double espera = micros(Reloj::now() - t0);
        res.espera_total_us += espera;
        res.espera_max_us = std::max(res.espera_max_us, espera);

        // Diagnóstico: después de entrar.
        long n = ++diag.lectoras_dentro;
        actualizar_maximo(diag.max_lectoras, n);
        if (diag.escritoras_dentro.load() != 0) ++diag.violaciones_exclusion;

        // Acceso al registro (intervalo admitido, fuera del monitor).
        long v = reg.version;
        long dv = reg.doble_version;
        if (dv != 2 * v) ++diag.inconsistencias;
        if (dur_us > 0) std::this_thread::sleep_for(std::chrono::microseconds(dur_us));

        // Diagnóstico: antes de salir.
        --diag.lectoras_dentro;
        ctl.salir_lectura();
        ++res.operaciones;
    }
    res.fallos = fallos_hebra;
}

static void hebra_escritora(int id, ControlRW& ctl, Registro& reg, Diagnostico& diag,
                            long K, long dur_us, ResultadoHebra& res) {
    id_hebra = id;
    fallos_hebra = ContadoresFallos{};
    for (long i = 0; i < K; ++i) {
        auto t0 = Reloj::now();
        ctl.entrar_escritura();
        double espera = micros(Reloj::now() - t0);
        res.espera_total_us += espera;
        res.espera_max_us = std::max(res.espera_max_us, espera);

        // Diagnóstico: después de entrar. Debe ser la única hebra dentro.
        long w = ++diag.escritoras_dentro;
        if (w != 1 || diag.lectoras_dentro.load() != 0) ++diag.violaciones_exclusion;

        reg.version++;
        reg.doble_version = 2 * reg.version;
        if (dur_us > 0) std::this_thread::sleep_for(std::chrono::microseconds(dur_us));

        // Diagnóstico: antes de salir.
        --diag.escritoras_dentro;
        ctl.salir_escritura();
        ++res.operaciones;
    }
    res.fallos = fallos_hebra;
}

static double cpu_ms() {
    timespec ts{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    Parametros p;
    if (!leer_parametros(argc, argv, p)) {
        uso(argv[0]);
        return 2;
    }

    const int L = static_cast<int>(p.L);
    const int E = static_cast<int>(p.E);
    const int N = L + E;

    MonitorRW* monitor = nullptr;
    std::unique_ptr<ControlRW> ctl = crear_control(p, monitor);
    Registro reg;
    Diagnostico diag;
    std::vector<ResultadoHebra> resultados(N);
    std::vector<std::thread> hebras;
    hebras.reserve(N);

    // Región de medición: desde antes de crear las hebras hasta completar los join().
    double cpu0 = cpu_ms();
    auto t0 = Reloj::now();

    bool error_creacion = false;
    // Ids lógicos: lectoras 0..L-1, escritoras L..L+E-1.
    for (int i = 0; i < N; ++i) {
        try {
            if (i < L)
                hebras.emplace_back(hebra_lectora, i, std::ref(*ctl), std::ref(reg), std::ref(diag),
                                    p.K, p.dur_lec_us, std::ref(resultados[i]));
            else
                hebras.emplace_back(hebra_escritora, i, std::ref(*ctl), std::ref(reg), std::ref(diag),
                                    p.K, p.dur_esc_us, std::ref(resultados[i]));
        } catch (const std::system_error& e) {
            std::fprintf(stderr, "Error: no se pudo crear la hebra %d: %s\n", i, e.what());
            error_creacion = true;
            break;
        }
    }
    auto t_creadas = Reloj::now();

    for (auto& h : hebras) h.join();

    auto t1 = Reloj::now();
    double cpu1 = cpu_ms();

    if (error_creacion) {
        std::fprintf(stderr, "Se esperó a las %zu hebras ya creadas; terminando.\n", hebras.size());
        return 1;
    }

    // Agregación de resultados por tipo.
    double esp_lec_total = 0, esp_lec_max = 0, esp_esc_total = 0, esp_esc_max = 0;
    long ops_lec = 0, ops_esc = 0;
    ContadoresFallos fallos_lec, fallos_esc;
    for (int i = 0; i < N; ++i) {
        const auto& r = resultados[i];
        if (i < L) {
            ops_lec += r.operaciones;
            esp_lec_total += r.espera_total_us;
            esp_lec_max = std::max(esp_lec_max, r.espera_max_us);
            fallos_lec.recurso_ocupado += r.fallos.recurso_ocupado;
            fallos_lec.escritoras_esperando += r.fallos.escritoras_esperando;
            fallos_lec.cas_fallido += r.fallos.cas_fallido;
        } else {
            ops_esc += r.operaciones;
            esp_esc_total += r.espera_total_us;
            esp_esc_max = std::max(esp_esc_max, r.espera_max_us);
            fallos_esc.recurso_ocupado += r.fallos.recurso_ocupado;
            fallos_esc.escritoras_esperando += r.fallos.escritoras_esperando;
            fallos_esc.cas_fallido += r.fallos.cas_fallido;
        }
    }

    const long esperado = p.E * p.K;
    const bool correcto = reg.version == esperado && reg.doble_version == 2 * esperado &&
                          diag.inconsistencias.load() == 0 && diag.violaciones_exclusion.load() == 0;

    std::printf("modo=%s\n", p.modo == Modo::Cond ? "cond" : "atomic");
    std::printf("politica=%s\n", nombre_politica(p.politica));
    std::printf("L=%ld E=%ld K=%ld dur_lec_us=%ld dur_esc_us=%ld traza=%s\n",
                p.L, p.E, p.K, p.dur_lec_us, p.dur_esc_us, p.con_traza ? "SI" : "NO");
    std::printf("operaciones=%ld lecturas=%ld escrituras=%ld\n", ops_lec + ops_esc, ops_lec, ops_esc);
    std::printf("version_final=%ld (esperado %ld)\n", reg.version, esperado);
    std::printf("doble_version_final=%ld (esperado %ld)\n", reg.doble_version, 2 * esperado);
    std::printf("inconsistencias=%ld\n", diag.inconsistencias.load());
    std::printf("violaciones_exclusion=%ld\n", diag.violaciones_exclusion.load());
    std::printf("max_lectoras_simultaneas=%ld\n", diag.max_lectoras.load());
    std::printf("tiempo_pared_ms=%.3f\n", micros(t1 - t0) / 1e3);
    std::printf("tiempo_creacion_hebras_ms=%.3f\n", micros(t_creadas - t0) / 1e3);
    std::printf("tiempo_cpu_ms=%.3f\n", cpu1 - cpu0);
    std::printf("espera_lectoras_prom_us=%.3f espera_lectoras_max_us=%.3f\n",
                ops_lec ? esp_lec_total / ops_lec : 0.0, esp_lec_max);
    std::printf("espera_escritoras_prom_us=%.3f espera_escritoras_max_us=%.3f\n",
                ops_esc ? esp_esc_total / ops_esc : 0.0, esp_esc_max);
    if (p.modo == Modo::Atomic) {
        std::printf("fallos_lectoras: recurso_ocupado=%ld escritoras_esperando=%ld cas_fallido=%ld\n",
                    fallos_lec.recurso_ocupado, fallos_lec.escritoras_esperando, fallos_lec.cas_fallido);
        std::printf("fallos_escritoras: recurso_ocupado=%ld cas_fallido=%ld\n",
                    fallos_esc.recurso_ocupado, fallos_esc.cas_fallido);
        std::printf("fallos_total=%ld\n",
                    fallos_lec.recurso_ocupado + fallos_lec.escritoras_esperando + fallos_lec.cas_fallido +
                    fallos_esc.recurso_ocupado + fallos_esc.cas_fallido);
    }
    if (monitor != nullptr && p.con_traza) {
        std::vector<AnotacionTraza> traza = monitor->obtener_traza();
        std::printf("anotaciones_traza=%zu (esperado %ld)\n", traza.size(), 3 * (p.L + p.E) * p.K);
        // Volcado opcional de la traza (para inspeccionarla a mano): RW_VOLCAR_TRAZA=archivo
        if (const char* ruta = std::getenv("RW_VOLCAR_TRAZA")) {
            if (FILE* f = std::fopen(ruta, "w")) {
                const char* acciones[] = {"Pide", "Entra", "Sale"};
                for (size_t i = 0; i < traza.size(); ++i) {
                    const auto& a = traza[i];
                    std::fprintf(f, "%zu hebra=%d %s %s turno=%ld\n", i, a.hebra,
                                 a.tipo == Tipo::Lectora ? "L" : "E",
                                 acciones[static_cast<int>(a.accion)], a.turno);
                }
                std::fclose(f);
            }
        }
    }
    std::printf("correcto=%s\n", correcto ? "SI" : "NO");
    return correcto ? 0 : 1;
}
