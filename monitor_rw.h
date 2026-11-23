// monitor_rw.h - Parte I: monitor de lectores y escritores con mutex y variables de condición.
#pragma once

#include "comun.h"

#include <condition_variable>
#include <mutex>
#include <vector>

// Reglas de monitor:
//  - Todo el estado de admisión es privado.
//  - Cada método público toma el mutex m al comenzar (std::unique_lock) y lo libera al
//    retornar; ninguno retorna manteniéndolo.
//  - Las esperas usan condition_variable::wait(lock, predicado), que libera m mientras
//    espera y vuelve a comprobar la condición al despertar (equivale a un while).
class MonitorRW : public ControlRW {
public:
    explicit MonitorRW(Politica p, bool con_traza = true);
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    std::vector<AnotacionTraza> obtener_traza();

private:
    Politica politica;
    bool con_traza;
    std::mutex m;                       // mutex del monitor
    std::vector<AnotacionTraza> traza;  // traza de admisión (R6)

    // Estado de admisión.
    long lectoras_activas = 0;      // entraron y no han salido
    bool escritora_activa = false;  // hay una escritora dentro
    long lectoras_esperando = 0;    // pidieron y no han entrado
    long escritoras_esperando = 0;  // pidieron y no han entrado

    // Variables de condición: una por tipo de hebra.
    std::condition_variable cv_lectoras;
    std::condition_variable cv_escritoras;

    // Condiciones de admisión según la política (se llaman con m adquirido).
    bool lectora_puede_entrar() const;
    bool escritora_puede_entrar() const;

    void anotar(Tipo t, Accion a, long turno = -1);
};
