// verificador.cpp - Implementación de verificar_traza (R6).
//
// Estado reconstruido al recorrer la traza en orden:
//   lectoras_activas / escritoras_activas    : entraron y no han salido
//   lectoras_pendientes / escritoras_pendientes : pidieron y no han entrado
//
// Reglas comprobadas en cada anotación Entra:
//   Todas:      una escritora entra sin lectoras ni escritora activas;
//               una lectora entra sin escritora activa.
//   Lectores:   una escritora entra sin lectoras pendientes.
//               (en cada Pide de lectora) si no hay escritora activa, la anotación
//               siguiente es su Entra.
//   Escritores: una lectora entra sin escritoras pendientes.
//   FIFO:       el k-ésimo Entra (desde 0) corresponde al turno k.
//
// Además se comprueba la coherencia de la traza: cada hebra alterna Pide -> Entra -> Sale
// con el mismo tipo, y en FIFO los Pide reciben turnos consecutivos desde 0.

#include "verificador.h"

#include <unordered_map>

namespace {

enum class Fase { Libre, Pidio, Dentro };

struct EstadoHebra {
    Fase fase = Fase::Libre;
    Tipo tipo = Tipo::Lectora;
    long turno = -1;
};

}  // namespace

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza, Politica politica) {
    ResultadoVerificacion r;
    switch (politica) {
        case Politica::Lectores:
            r.descripcion_evidencia = "lectoras que entraron con escritoras pendientes";
            break;
        case Politica::Escritores:
            r.descripcion_evidencia = "lectoras que pidieron con escritoras pendientes";
            break;
        case Politica::Fifo:
            r.descripcion_evidencia = "lectoras que entraron con otra lectora activa";
            break;
    }

    auto violar = [&](size_t i, const std::string& regla) {
        r.politica_ok = false;
        ++r.total_violaciones;
        if (r.violaciones.size() < ResultadoVerificacion::MAX_GUARDADAS) r.violaciones.push_back({i, regla});
    };

    long lectoras_activas = 0, escritoras_activas = 0;
    long lectoras_pendientes = 0, escritoras_pendientes = 0;
    long entras = 0;           // cuántos Entra se han visto (k)
    long siguiente_turno = 0;  // FIFO: turno esperado en el próximo Pide
    std::unordered_map<int, EstadoHebra> hebras;

    for (size_t i = 0; i < traza.size(); ++i) {
        const AnotacionTraza& a = traza[i];
        const bool lectora = a.tipo == Tipo::Lectora;
        EstadoHebra& h = hebras[a.hebra];

        switch (a.accion) {
            case Accion::Pide: {
                if (h.fase != Fase::Libre) violar(i, "coherencia: la hebra pide con una solicitud en curso");
                h.fase = Fase::Pidio;
                h.tipo = a.tipo;
                h.turno = a.turno;
                if (politica == Politica::Fifo) {
                    if (a.turno != siguiente_turno)
                        violar(i, "FIFO: los turnos no son consecutivos (se esperaba " +
                                      std::to_string(siguiente_turno) + ", llegó " + std::to_string(a.turno) + ")");
                    siguiente_turno = a.turno + 1;
                }

                if (lectora) {
                    if (politica == Politica::Escritores && escritoras_pendientes > 0) ++r.evidencia;
                    if (politica == Politica::Lectores && escritoras_activas == 0) {
                        const bool siguiente_es_su_entra =
                            i + 1 < traza.size() && traza[i + 1].hebra == a.hebra &&
                            traza[i + 1].accion == Accion::Entra && traza[i + 1].tipo == Tipo::Lectora;
                        if (!siguiente_es_su_entra)
                            violar(i, "Lectores: lectora pide sin escritora activa y la anotación siguiente no es su Entra");
                    }
                    ++lectoras_pendientes;
                } else {
                    ++escritoras_pendientes;
                }
                break;
            }

            case Accion::Entra: {
                if (h.fase != Fase::Pidio || h.tipo != a.tipo)
                    violar(i, "coherencia: Entra sin un Pide previo del mismo tipo");
                h.fase = Fase::Dentro;

                if (lectora) {
                    --lectoras_pendientes;
                    // Regla común.
                    if (escritoras_activas > 0) violar(i, "Todas: lectora entra con escritora activa");
                    // Reglas por política.
                    if (politica == Politica::Escritores && escritoras_pendientes > 0)
                        violar(i, "Escritores: lectora entra con escritoras pendientes");
                    // Evidencia.
                    if (politica == Politica::Lectores && escritoras_pendientes > 0) ++r.evidencia;
                    if (politica == Politica::Fifo && lectoras_activas > 0) ++r.evidencia;
                    ++lectoras_activas;
                } else {
                    --escritoras_pendientes;
                    if (lectoras_activas > 0 || escritoras_activas > 0)
                        violar(i, "Todas: escritora entra con lectoras o escritora activas");
                    if (politica == Politica::Lectores && lectoras_pendientes > 0)
                        violar(i, "Lectores: escritora entra con lectoras pendientes");
                    ++escritoras_activas;
                }

                if (politica == Politica::Fifo && a.turno != entras)
                    violar(i, "FIFO: el Entra número " + std::to_string(entras) + " tiene turno " +
                                  std::to_string(a.turno));
                ++entras;
                break;
            }

            case Accion::Sale: {
                if (h.fase != Fase::Dentro || h.tipo != a.tipo)
                    violar(i, "coherencia: Sale sin un Entra previo del mismo tipo");
                h.fase = Fase::Libre;
                if (lectora) --lectoras_activas;
                else --escritoras_activas;
                break;
            }
        }
    }

    // Al final todas las solicitudes deben haber salido.
    if (lectoras_activas != 0 || escritoras_activas != 0 || lectoras_pendientes != 0 || escritoras_pendientes != 0)
        violar(traza.size(), "coherencia: la traza termina con solicitudes pendientes o activas");

    return r;
}
