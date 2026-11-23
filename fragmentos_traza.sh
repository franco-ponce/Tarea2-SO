#!/usr/bin/env bash
# Fragmentos de traza para el anexo del informe (E1): para cada política del modo cond,
# ejecuta una configuración con varias lectoras, guarda la traza completa y extrae un
# fragmento breve donde se observa la regla de prioridad.
#
#   lectores:   una escritora espera (Pide) y una lectora que pide después entra antes que ella.
#   escritores: una lectora pide mientras una escritora espera, y entra después de ella.
#   fifo:       una escritora con turno t bloquea a una lectora con turno t+1, y luego
#               entran lectoras consecutivas mientras otra lectora está activa.
#
# Salidas en resultados/trazas/: traza_<politica>.txt (completa) y fragmento_<politica>.txt
set -u
cd "$(dirname "$0")"
DIR=resultados/trazas
mkdir -p "$DIR"
make -s rw || exit 1

CFG="6 2 10 200 500"

for pol in lectores escritores fifo; do
  traza="$DIR/traza_$pol.txt"
  RW_VOLCAR_TRAZA="$traza" ./rw cond "$pol" $CFG > "$DIR/salida_$pol.txt"
  # Formato de cada línea: <indice> hebra=<id> <L|E> <Pide|Entra|Sale> turno=<t>
  awk -v pol="$pol" '
  { idx[NR] = $1; hebra[NR] = $2; tipo[NR] = $3; acc[NR] = $4; linea[NR] = $0 }
  END {
    pe = 0; al = 0; ini = 0; fin = 0
    for (i = 1; i <= NR; i++) {
      if (acc[i] == "Pide" && tipo[i] == "E") { pe++; if (!esc_pide) esc_pide = i }
      if (acc[i] == "Entra" && tipo[i] == "E") { pe--; if (pe == 0) esc_pide = 0 }
      if (acc[i] == "Entra" && tipo[i] == "L") al++
      if (acc[i] == "Sale"  && tipo[i] == "L") al--

      if (pol == "lectores" && acc[i] == "Entra" && tipo[i] == "L" && pe > 0 && esc_pide && i - esc_pide > 1) {
        ini = esc_pide; fin = i; break
      }
      if (pol == "escritores" && acc[i] == "Pide" && tipo[i] == "L" && pe > 0) {
        # Muestra: la escritora pide, la lectora pide, entra la escritora ... y mucho después
        # entra la lectora (se omite el tramo intermedio).
        for (j = i + 1; j <= NR; j++) if (hebra[j] == hebra[i] && acc[j] == "Entra") break
        for (w = i + 1; w <= NR; w++) if (tipo[w] == "E" && acc[w] == "Entra") break
        a = esc_pide > 3 ? esc_pide - 3 : 1
        for (x = a; x <= w + 1 && x <= NR; x++) print linea[x]
        if (j - 3 > w + 2) print "..."
        for (x = (j - 3 > w + 1 ? j - 3 : w + 2); x <= j && x <= NR; x++) print linea[x]
        hecho = 1; exit
      }
      if (pol == "fifo" && acc[i] == "Pide" && tipo[i] == "L" && pe > 0) {
        for (j = i + 1; j <= NR; j++) if (hebra[j] == hebra[i] && acc[j] == "Entra") break
        # Incluye algunas anotaciones más para mostrar lectoras consecutivas concurrentes.
        ini = esc_pide; fin = j + 4; break
      }
    }
    if (hecho) exit
    if (!ini) { print "(no se encontró un fragmento representativo en esta ejecución)"; exit }
    if (ini > 3) ini -= 3
    if (fin - ini > 40) fin = ini + 40
    if (fin > NR) fin = NR
    for (i = ini; i <= fin; i++) print linea[i]
  }' "$traza" > "$DIR/fragmento_$pol.txt"
  echo "=== $pol ($(grep -E '^(politica_ok|evidencia)=' "$DIR/salida_$pol.txt" | tr '\n' ' '))"
  cat "$DIR/fragmento_$pol.txt"
done
