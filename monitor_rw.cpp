// monitor_rw.cpp - Implementación de MonitorRW (Parte I).
//
// Política A, prioridad de lectores (R3):
//   lectora entra  <=> no hay escritora activa
//   escritora entra <=> no hay escritora activa, ni lectoras activas, ni lectoras pendientes
//
// Política B, prioridad de escritores (R4):
//   lectora entra  <=> no hay escritora activa ni escritoras pendientes
//   escritora entra <=> no hay escritora activa ni lectoras activas
//
// Política C, FIFO (R5): cada solicitud recibe un turno consecutivo al registrarse (Pide).
//   turno_en_atencion = menor turno que todavía no entra (todos los anteriores ya entraron).
//   lectora con turno t entra  <=> t == turno_en_atencion y no hay escritora activa
//   escritora con turno t entra <=> t == turno_en_atencion, sin escritora ni lectoras activas
//   Al entrar, turno_en_atencion avanza a t+1. Así:
//     - los ingresos ocurren exactamente en orden de turno;
//     - lectoras consecutivas entran una tras otra sin esperar a que salgan las anteriores,
//       por lo que quedan dentro simultáneamente;
//     - una escritora con turno t espera a que salgan las lectoras admitidas antes que ella,
//       y mientras espera, turno_en_atencion == t bloquea a todas las solicitudes posteriores.
//   El orden lo impone el contador, no el orden en que el sistema entrega el mutex ni el
//   orden de notify. Si las hebras progresan, la solicitud t entra después de un número
//   finito de ingresos (los turnos menores a t), así que no hay adelantos indefinidos.
//
// "Pendiente" = pidió (anotación Pide) y aún no entra. Cada solicitud se cuenta como
// pendiente desde que toma el mutex en entrar_*() hasta justo antes de su Entra, de modo
// que el estado del monitor coincide con lo que reconstruye el verificador de la traza.
//
// Avisos:
//  - Lectores/Escritores: las lectoras se despiertan con notify_all (pueden entrar varias a
//    la vez). A las escritoras basta con notify_one: todas esperan exactamente la misma
//    condición, así que cualquiera que despierte puede usar el recurso. Si al despertar otra
//    hebra ya lo tomó, vuelve a esperar, y la que lo tomó avisará de nuevo al salir; por eso
//    un aviso nunca deja bloqueada a una hebra que sí podría entrar.
//  - FIFO: aviso por solicitud. Solo la solicitud con turno == turno_en_atencion puede
//    entrar, así que después de cada cambio de estado se notifica únicamente su variable
//    de condición (si está esperando).

#include "monitor_rw.h"

// Turno de la solicitud en curso de esta hebra (FIFO), para anotarlo también en Sale.
// Una hebra tiene a lo más una solicitud en curso a la vez.
static thread_local long turno_hebra = -1;

MonitorRW::MonitorRW(Politica p, bool con_traza_) : politica(p), con_traza(con_traza_) {}

void MonitorRW::anotar(Tipo t, Accion a, long turno) {
    if (con_traza) traza.push_back(AnotacionTraza{id_hebra, t, a, turno});
}

bool MonitorRW::lectora_puede_entrar(long turno) const {
    switch (politica) {
        case Politica::Lectores:
            return !escritora_activa;
        case Politica::Escritores:
            return !escritora_activa && escritoras_esperando == 0;
        case Politica::Fifo:
            return turno == turno_en_atencion && !escritora_activa;
    }
    return false;
}

bool MonitorRW::escritora_puede_entrar(long turno) const {
    switch (politica) {
        case Politica::Lectores:
            return !escritora_activa && lectoras_activas == 0 && lectoras_esperando == 0;
        case Politica::Escritores:
            return !escritora_activa && lectoras_activas == 0;
        case Politica::Fifo:
            return turno == turno_en_atencion && !escritora_activa && lectoras_activas == 0;
    }
    return false;
}

void MonitorRW::avisar_turno_en_atencion() {
    auto it = espera_por_turno.find(turno_en_atencion);
    if (it != espera_por_turno.end()) it->second->notify_one();
}

void MonitorRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m);
    long turno = -1;
    if (politica == Politica::Fifo) turno = siguiente_turno++;
    turno_hebra = turno;
    anotar(Tipo::Lectora, Accion::Pide, turno);
    ++lectoras_esperando;

    if (politica == Politica::Fifo) {
        if (!lectora_puede_entrar(turno)) {
            std::condition_variable mi_cv;
            espera_por_turno[turno] = &mi_cv;
            mi_cv.wait(lk, [&] { return lectora_puede_entrar(turno); });
            espera_por_turno.erase(turno);
        }
    } else {
        cv_lectoras.wait(lk, [&] { return lectora_puede_entrar(turno); });
    }

    --lectoras_esperando;
    ++lectoras_activas;
    anotar(Tipo::Lectora, Accion::Entra, turno);

    if (politica == Politica::Fifo) {
        // Avanza el turno: si la siguiente solicitud es una lectora, puede entrar ya
        // (lectoras consecutivas concurrentes).
        ++turno_en_atencion;
        avisar_turno_en_atencion();
    }
}

void MonitorRW::salir_lectura() {
    std::unique_lock<std::mutex> lk(m);
    --lectoras_activas;
    anotar(Tipo::Lectora, Accion::Sale, turno_hebra);

    if (politica == Politica::Fifo) {
        // Solo puede habilitar a una escritora en turno que espera que salgan las lectoras.
        if (lectoras_activas == 0) avisar_turno_en_atencion();
    } else {
        // La última lectora deja el recurso libre. En prioridad de lectores la escritora
        // además necesita que no haya lectoras pendientes; la condición lo vuelve a comprobar.
        if (lectoras_activas == 0 && escritoras_esperando > 0) cv_escritoras.notify_one();
    }
}

void MonitorRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m);
    long turno = -1;
    if (politica == Politica::Fifo) turno = siguiente_turno++;
    turno_hebra = turno;
    anotar(Tipo::Escritora, Accion::Pide, turno);
    ++escritoras_esperando;

    if (politica == Politica::Fifo) {
        if (!escritora_puede_entrar(turno)) {
            std::condition_variable mi_cv;
            espera_por_turno[turno] = &mi_cv;
            mi_cv.wait(lk, [&] { return escritora_puede_entrar(turno); });
            espera_por_turno.erase(turno);
        }
    } else {
        cv_escritoras.wait(lk, [&] { return escritora_puede_entrar(turno); });
    }

    --escritoras_esperando;
    escritora_activa = true;
    anotar(Tipo::Escritora, Accion::Entra, turno);

    if (politica == Politica::Fifo) {
        // La siguiente solicitud no puede entrar mientras esta escritora esté activa;
        // se le avisará en salir_escritura().
        ++turno_en_atencion;
    }
}

void MonitorRW::salir_escritura() {
    std::unique_lock<std::mutex> lk(m);
    escritora_activa = false;
    anotar(Tipo::Escritora, Accion::Sale, turno_hebra);

    switch (politica) {
        case Politica::Lectores:
            // Las lectoras pendientes tienen prioridad; si no hay, pasa una escritora.
            if (lectoras_esperando > 0) cv_lectoras.notify_all();
            else if (escritoras_esperando > 0) cv_escritoras.notify_one();
            break;
        case Politica::Escritores:
            // Primero las escritoras pendientes; las lectoras solo cuando no queda ninguna.
            if (escritoras_esperando > 0) cv_escritoras.notify_one();
            else if (lectoras_esperando > 0) cv_lectoras.notify_all();
            break;
        case Politica::Fifo:
            // El recurso queda libre: puede entrar la solicitud en turno, sea del tipo que sea.
            avisar_turno_en_atencion();
            break;
    }
}

std::vector<AnotacionTraza> MonitorRW::obtener_traza() {
    std::unique_lock<std::mutex> lk(m);
    return traza;
}
