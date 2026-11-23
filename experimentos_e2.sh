#!/usr/bin/env bash
# E2. Costo de sincronización: política escritores, modo cond vs modo atomic, sin traza.
# Configuraciones de E1; la configuración sin pausas usa K = 10000. REPS ejecuciones
# (por defecto 5) de cada caso y promedio.
#
# Salidas (en resultados/e2/):
#   salidas/<modo>_<L>_<E>_<K>_<dl>_<de>_r<n>.txt   salida completa de cada ejecución
#   e2.csv                                          una fila por ejecución
#   promedios_e2.csv / promedios_e2.txt             promedio de las REPS ejecuciones por caso
set -u
cd "$(dirname "$0")"
REPS=${REPS:-5}
DIR=resultados/e2
mkdir -p "$DIR/salidas"
make -s rw || exit 1

CONFIGS=("1 1 20 500 500" "6 2 100 200 500" "2 6 100 500 200" "8 8 10000 0 0" "8 1 200 1000 100")
MODOS=(cond atomic)

valor() { grep -oE "(^| )$1=[^ ]*" "$2" | head -1 | cut -d= -f2; }

CSV="$DIR/e2.csv"
echo "modo,L,E,K,dur_lec_us,dur_esc_us,rep,correcto,tiempo_pared_ms,tiempo_creacion_hebras_ms,tiempo_cpu_ms,espera_lectoras_prom_us,espera_lectoras_max_us,espera_escritoras_prom_us,espera_escritoras_max_us,max_lectoras_simultaneas,fallos_total" > "$CSV"

errores=0
for cfg in "${CONFIGS[@]}"; do
  read -r L E K DL DE <<< "$cfg"
  for modo in "${MODOS[@]}"; do
    for ((r = 1; r <= REPS; r++)); do
      f="$DIR/salidas/${modo}_${L}_${E}_${K}_${DL}_${DE}_r${r}.txt"
      ./rw "$modo" escritores $cfg notraza > "$f" 2>&1 || errores=$((errores + 1))
      ft=$(valor fallos_total "$f"); ft=${ft:-0}
      echo "$modo,$L,$E,$K,$DL,$DE,$r,$(valor correcto "$f"),$(valor tiempo_pared_ms "$f"),$(valor tiempo_creacion_hebras_ms "$f"),$(valor tiempo_cpu_ms "$f"),$(valor espera_lectoras_prom_us "$f"),$(valor espera_lectoras_max_us "$f"),$(valor espera_escritoras_prom_us "$f"),$(valor espera_escritoras_max_us "$f"),$(valor max_lectoras_simultaneas "$f"),$ft" >> "$CSV"
    done
    echo "listo: $modo $cfg"
  done
done

# Promedios por (modo, configuración). Columnas 9..17 son numéricas.
PROM="$DIR/promedios_e2.csv"
awk -F, -v OFS=, '
NR == 1 { printf "modo,L,E,K,dur_lec_us,dur_esc_us,ejecuciones,todas_correctas"; for (c = 9; c <= NF; c++) printf ",%s", $c; printf "\n"; nc = NF; next }
{
  clave = $1 OFS $2 OFS $3 OFS $4 OFS $5 OFS $6
  if (!(clave in n)) orden[++k] = clave
  n[clave]++
  if ($8 != "SI") malas[clave]++
  for (c = 9; c <= NF; c++) suma[clave, c] += $c
}
END {
  for (i = 1; i <= k; i++) {
    clave = orden[i]
    printf "%s,%d,%s", clave, n[clave], (malas[clave] ? "NO" : "SI")
    for (c = 9; c <= nc; c++) printf ",%.3f", suma[clave, c] / n[clave]
    printf "\n"
  }
}' "$CSV" > "$PROM"
column -t -s, "$PROM" > "$DIR/promedios_e2.txt"

echo
cat "$DIR/promedios_e2.txt"
echo
echo "Ejecuciones con error o correcto=NO: $errores"
[ "$errores" -eq 0 ]
