// verificador.h - Verificador de la traza de admisión (R6).
#pragma once

#include "comun.h"

#include <string>
#include <vector>

struct Violacion {
    size_t indice;      // número de la anotación (posición en la traza, desde 0)
    std::string regla;  // regla incumplida
};

struct ResultadoVerificacion {
    bool politica_ok = true;
    long total_violaciones = 0;
    std::vector<Violacion> violaciones;  // se guardan solo las primeras (ver MAX_GUARDADAS)
    // Indicador de evidencia: cuántas veces la regla propia de la política llegó a ponerse
    // a prueba en esta ejecución.
    //   Lectores:   lectoras que entraron mientras había escritoras pendientes.
    //   Escritores: lectoras que pidieron mientras había escritoras pendientes.
    //   FIFO:       lectoras que entraron mientras otra lectora estaba activa.
    long evidencia = 0;
    const char* descripcion_evidencia = "";

    static constexpr size_t MAX_GUARDADAS = 20;
};

// Recorre la traza reconstruyendo lectoras/escritoras activas y pendientes y comprueba,
// en cada Entra, las reglas comunes y las de la política.
ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza, Politica politica);
