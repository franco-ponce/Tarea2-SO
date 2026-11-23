// comun.h - Tipos compartidos por todas las implementaciones de lectores/escritores.
#pragma once

#include <vector>

// Interfaz común (enunciado, Sección 1). Monitor y versión atómica derivan de ella,
// de modo que hebras, registro y medición son los mismos para ambas.
class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;
};

enum class Politica { Lectores, Escritores, Fifo };

// Registro compartido (R2). Se accede sin sincronización propia: solo dentro del
// intervalo que admite el protocolo de lectores y escritores.
struct Registro {
    long version = 0;
    long doble_version = 0;
};

// Traza de admisión (R6).
enum class Tipo { Lectora, Escritora };
enum class Accion { Pide, Entra, Sale };

struct AnotacionTraza {
    int hebra;    // identificador lógico de la hebra
    Tipo tipo;
    Accion accion;
    long turno;   // turno FIFO; -1 en las otras políticas
};

// Identificador lógico de la hebra actual; cada hebra lo asigna al comenzar (R6).
extern thread_local int id_hebra;

// Intentos de admisión fallidos del modo atomic, por causa (R10). Son contadores
// locales de cada hebra; la hebra los copia a su resultado antes de terminar y
// rw.cpp los suma después de los join().
struct ContadoresFallos {
    long recurso_ocupado = 0;      // escritora activa o, para una escritora, lectoras activas
    long escritoras_esperando = 0; // solo lectoras
    long cas_fallido = 0;          // CAS que falló (otra hebra cambió la palabra o fallo espurio)
};
extern thread_local ContadoresFallos fallos_hebra;

const char* nombre_politica(Politica p);
