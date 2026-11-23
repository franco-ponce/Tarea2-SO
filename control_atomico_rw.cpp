// control_atomico_rw.cpp - Implementación de ControlAtomicoRW (Parte II, política de escritores).
//
// Protocolo (s = valor leído de la palabra):
//
//   entrar_lectura:   repetir
//                       si EA(s) = 1                -> fallo "recurso ocupado", reintentar
//                       si escritoras_esp(s) > 0    -> fallo "escritoras esperando", reintentar
//                       CAS(s -> s + 1 lectora)     -> éxito: admitida; fallo: "CAS fallido"
//   salir_lectura:    fetch_sub(1 lectora)
//
//   entrar_escritura: fetch_add(1 esperando)        -> desde aquí ninguna lectora nueva entra
//                     repetir
//                       si EA(s) = 1 o lectoras(s) > 0 -> fallo "recurso ocupado", reintentar
//                       CAS(s -> s - 1 esperando + EA) -> éxito: admitida; fallo: "CAS fallido"
//   salir_escritura:  fetch_sub(EA)
//
// Punto de decisión atómico: la condición se evalúa sobre el valor s, y el CAS solo tiene
// éxito si la palabra sigue valiendo exactamente s. Si entre la lectura y el CAS otra hebra
// registró una escritora esperando, entró o salió, la palabra cambió, el CAS falla y la
// condición se vuelve a evaluar con el valor nuevo. Así no puede entrar una lectora
// "habiendo visto" que no había escritoras cuando en realidad ya había una registrada.
//
// Prioridad de escritores (R4): una escritora queda registrada en escritoras_esp antes de
// su primer intento; desde ese momento todo CAS de lectora falla o ni se intenta. Las
// lectoras activas terminan normalmente (salir_lectura no está condicionado) y cuando el
// recurso queda libre solo las escritoras pueden ganar el CAS mientras quede alguna
// esperando.
//
// Espera: activa. Una hebra que no puede entrar vuelve a leer la palabra y reintenta,
// llamando a std::this_thread::yield() para ceder la CPU a otras hebras (útil cuando hay
// más hebras que núcleos). Tras un CAS fallido se reintenta de inmediato, porque el CAS ya
// dejó en s el valor actual de la palabra.
//
// Orden de memoria: se usan las operaciones con su orden por defecto (seq_cst). El CAS
// exitoso de entrada actúa como adquisición y la fetch_sub de salida como liberación, de
// modo que los accesos al Registro de una escritora "ocurren antes" que los de la hebra
// admitida después; no hay carreras de datos sobre el Registro.
//
// Contadores de fallos (R10): fallos_hebra es thread_local, por lo que cada hebra cuenta
// sin compartir nada; rw.cpp los suma después de los join().

#include "control_atomico_rw.h"

#include <thread>

void ControlAtomicoRW::entrar_lectura() {
    std::uint64_t s = estado.load();
    for (;;) {
        if (escritora_activa(s)) {
            ++fallos_hebra.recurso_ocupado;  // si además hay esperando, cuenta como ocupado
            std::this_thread::yield();
            s = estado.load();
            continue;
        }
        if (esperando(s) > 0) {
            ++fallos_hebra.escritoras_esperando;
            std::this_thread::yield();
            s = estado.load();
            continue;
        }
        if (estado.compare_exchange_weak(s, s + UNA_LECTORA)) return;  // admitida
        ++fallos_hebra.cas_fallido;  // s ya tiene el valor actual
    }
}

void ControlAtomicoRW::salir_lectura() {
    estado.fetch_sub(UNA_LECTORA);
}

void ControlAtomicoRW::entrar_escritura() {
    // Registrarse como esperando antes de intentar la admisión.
    std::uint64_t s = estado.fetch_add(UNA_ESPERANDO) + UNA_ESPERANDO;
    for (;;) {
        if (escritora_activa(s) || lectoras(s) > 0) {
            ++fallos_hebra.recurso_ocupado;
            std::this_thread::yield();
            s = estado.load();
            continue;
        }
        // El mismo CAS la descuenta de las esperando y la marca como activa.
        if (estado.compare_exchange_weak(s, s - UNA_ESPERANDO + ESCRITORA_ACTIVA)) return;  // admitida
        ++fallos_hebra.cas_fallido;
    }
}

void ControlAtomicoRW::salir_escritura() {
    estado.fetch_sub(ESCRITORA_ACTIVA);
}
