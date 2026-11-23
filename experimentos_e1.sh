#!/usr/bin/env bash
# E1. Corrección y prioridades.
# Ejecuta las tres políticas del modo cond y la política escritores del modo atomic,
# REPS veces (por defecto 5) para cada configuración, con la traza activada.
#
# Salidas (en resultados/e1/):
#   salidas/<modo>_<politica>_<L>_<E>_<K>_<dl>_<de>_r<n>.txt   salida completa de cada ejecución
#   e1.csv                                                      una fila por ejecución
#   resumen_e1.txt                                              tabla legible (todas las filas)
set -u
cd "$(dirname "$0")"
REPS=${REPS:-5}
DIR=resultados/e1
mkdir -p "$DIR/salidas"
make -s rw || exit 1

CONFIGS=("1 1 20 500 500" "6 2 100 200 500" "2 6 100 500 200" "8 8 100 0 0" "8 1 200 1000 100")
VARIANTES=("cond lectores" "cond escritores" "cond fifo" "atomic escritores")

# valor <clave> <archivo>: extrae el valor de "clave=valor" de la salida de rw.
valor() { grep -oE "(^| )$1=[^ ]*" "$2" | head -1 | cut -d= -f2; }

CSV="$DIR/e1.csv"
echo "modo,politica,L,E,K,dur_lec_us,dur_esc_us,rep,correcto,version_final,doble_version_final,inconsistencias,violaciones_exclusion,max_lectoras_simultaneas,politica_ok,evidencia,fallos_lec_recurso_ocupado,fallos_lec_escritoras_esperando,fallos_lec_cas,fallos_esc_recurso_ocupado,fallos_esc_cas" > "$CSV"

errores=0
for var in "${VARIANTES[@]}"; do
  read -r modo pol <<< "$var"
  for cfg in "${CONFIGS[@]}"; do
    read -r L E K DL DE <<< "$cfg"
    for ((r = 1; r <= REPS; r++)); do
      f="$DIR/salidas/${modo}_${pol}_${L}_${E}_${K}_${DL}_${DE}_r${r}.txt"
      ./rw "$modo" "$pol" $cfg > "$f" 2>&1 || errores=$((errores + 1))
      if [ "$modo" = atomic ]; then
        pok="-"; ev="-"
        flr=$(grep 'fallos_lectoras' "$f" | grep -oE 'recurso_ocupado=[0-9]+' | cut -d= -f2)
        fle=$(valor escritoras_esperando "$f")
        flc=$(grep 'fallos_lectoras' "$f" | grep -oE 'cas_fallido=[0-9]+' | cut -d= -f2)
        fer=$(grep 'fallos_escritoras' "$f" | grep -oE 'recurso_ocupado=[0-9]+' | cut -d= -f2)
        fec=$(grep 'fallos_escritoras' "$f" | grep -oE 'cas_fallido=[0-9]+' | cut -d= -f2)
      else
        pok=$(valor politica_ok "$f"); ev=$(valor evidencia "$f")
        flr="-"; fle="-"; flc="-"; fer="-"; fec="-"
      fi
      echo "$modo,$pol,$L,$E,$K,$DL,$DE,$r,$(valor correcto "$f"),$(valor version_final "$f"),$(valor doble_version_final "$f"),$(valor inconsistencias "$f"),$(valor violaciones_exclusion "$f"),$(valor max_lectoras_simultaneas "$f"),$pok,$ev,$flr,$fle,$flc,$fer,$fec" >> "$CSV"
    done
    echo "listo: $modo $pol $cfg"
  done
done

column -t -s, "$CSV" > "$DIR/resumen_e1.txt"
echo
echo "Ejecuciones con correcto=NO o politica_ok=NO (o error): $errores"
echo "CSV:     $CSV"
echo "Resumen: $DIR/resumen_e1.txt"
[ "$errores" -eq 0 ]
