// control_atomico_rw.h - Parte II: prioridad de escritores con operaciones atómicas y CAS.
#pragma once

#include "comun.h"

#include <atomic>
#include <cstdint>

// No es un monitor: no hay mutex ni variables de condición, y sus métodos pueden
// ejecutarse concurrentemente. Todo el estado de sincronización está en una única palabra
// atómica de 64 bits:
//
//   bit 63 ................ 48 47 ............ 32 31 ...... 17   16   15 ............ 0
//   [     sin uso (0)       ][ escritoras_esp  ][  sin uso  ][ EA ][ lectoras_activas ]
//
//   lectoras_activas  (bits 0-15,  16 bits): lectoras dentro del recurso.
//   EA                (bit 16,      1 bit) : 1 si hay una escritora dentro.
//   escritoras_esp    (bits 32-47, 16 bits): escritoras registradas esperando admisión.
//
// Desborde: por R1, L + E <= 1000 con L, E >= 1, así que L <= 999 y E <= 999. Cada hebra
// tiene a lo más una solicitud en curso, por lo que lectoras_activas <= L <= 999 y
// escritoras_esp <= E <= 999, ambos muy por debajo de 2^16 - 1 = 65535. EA vale 0 o 1
// porque solo se pone en 1 con un CAS que exige EA == 0. Los bits libres entre campos
// son margen adicional.
class ControlAtomicoRW : public ControlRW {
public:
    ControlAtomicoRW() = default;
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    // Para pruebas: lee la palabra (al final debe ser 0).
    std::uint64_t palabra() const { return estado.load(); }

    static constexpr std::uint64_t UNA_LECTORA      = 1ULL;        // incremento de lectoras_activas
    static constexpr std::uint64_t MASCARA_LECTORAS = 0xFFFFULL;
    static constexpr std::uint64_t ESCRITORA_ACTIVA = 1ULL << 16;  // bit EA
    static constexpr int           DESPL_ESPERANDO  = 32;
    static constexpr std::uint64_t UNA_ESPERANDO    = 1ULL << DESPL_ESPERANDO;
    static constexpr std::uint64_t MASCARA_ESPERANDO = 0xFFFFULL << DESPL_ESPERANDO;

    static std::uint64_t lectoras(std::uint64_t s) { return s & MASCARA_LECTORAS; }
    static bool escritora_activa(std::uint64_t s) { return (s & ESCRITORA_ACTIVA) != 0; }
    static std::uint64_t esperando(std::uint64_t s) { return (s & MASCARA_ESPERANDO) >> DESPL_ESPERANDO; }

private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> estado{0};
};
