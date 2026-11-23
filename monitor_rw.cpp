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
// "Pendiente" = pidió (anotación Pide) y aún no entra. Cada solicitud se cuenta como
// pendiente desde que toma el mutex en entrar_*() hasta justo antes de su Entra, de modo
// que el estado del monitor coincide con lo que reconstruye el verificador de la traza.
//
// Avisos: las lectoras se despiertan con notify_all (pueden entrar varias a la vez). A las
// escritoras basta con notify_one: todas esperan exactamente la misma condición, así que
// cualquiera que despierte puede usar el recurso. Si al despertar otra hebra ya lo tomó,
// vuelve a esperar, y la hebra que lo tomó avisará de nuevo cuando salga; por eso un aviso
// nunca se pierde dejando bloqueada a una hebra que sí podría entrar.

#include "monitor_rw.h"

MonitorRW::MonitorRW(Politica p, bool con_traza_) : politica(p), con_traza(con_traza_) {}

void MonitorRW::anotar(Tipo t, Accion a, long turno) {
    if (con_traza) traza.push_back(AnotacionTraza{id_hebra, t, a, turno});
}

bool MonitorRW::lectora_puede_entrar() const {
    switch (politica) {
        case Politica::Lectores:
            return !escritora_activa;
        case Politica::Escritores:
            return !escritora_activa && escritoras_esperando == 0;
        case Politica::Fifo:
            break;  // Etapa 3
    }
    return false;
}

bool MonitorRW::escritora_puede_entrar() const {
    switch (politica) {
        case Politica::Lectores:
            return !escritora_activa && lectoras_activas == 0 && lectoras_esperando == 0;
        case Politica::Escritores:
            return !escritora_activa && lectoras_activas == 0;
        case Politica::Fifo:
            break;  // Etapa 3
    }
    return false;
}

void MonitorRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m);
    anotar(Tipo::Lectora, Accion::Pide);
    ++lectoras_esperando;
    cv_lectoras.wait(lk, [this] { return lectora_puede_entrar(); });
    --lectoras_esperando;
    ++lectoras_activas;
    anotar(Tipo::Lectora, Accion::Entra);
}

void MonitorRW::salir_lectura() {
    std::unique_lock<std::mutex> lk(m);
    --lectoras_activas;
    anotar(Tipo::Lectora, Accion::Sale);
    // La última lectora deja el recurso libre. En prioridad de lectores la escritora
    // además necesita que no haya lectoras pendientes; la condición lo vuelve a comprobar.
    if (lectoras_activas == 0 && escritoras_esperando > 0) cv_escritoras.notify_one();
}

void MonitorRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m);
    anotar(Tipo::Escritora, Accion::Pide);
    ++escritoras_esperando;
    cv_escritoras.wait(lk, [this] { return escritora_puede_entrar(); });
    --escritoras_esperando;
    escritora_activa = true;
    anotar(Tipo::Escritora, Accion::Entra);
}

void MonitorRW::salir_escritura() {
    std::unique_lock<std::mutex> lk(m);
    escritora_activa = false;
    anotar(Tipo::Escritora, Accion::Sale);

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
            break;  // Etapa 3
    }
}

std::vector<AnotacionTraza> MonitorRW::obtener_traza() {
    std::unique_lock<std::mutex> lk(m);
    return traza;
}
