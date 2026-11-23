#!/usr/bin/env bash
# Comprueba que ./rw rechaza parámetros inválidos y combinaciones no implementadas (R1).
cd "$(dirname "$0")"
casos=(
  "atomic lectores 1 1 1 0 0"
  "atomic fifo 1 1 1 0 0"
  "otro escritores 1 1 1 0 0"
  "cond x 1 1 1 0 0"
  "cond fifo 0 1 1 0 0"
  "cond fifo 1 0 1 0 0"
  "cond fifo 500 501 1 0 0"
  "cond fifo 1 1 0 0 0"
  "cond fifo 1 1 1abc 0 0"
  "cond fifo 1 1 1 -1 0"
  "cond fifo 1 1 1 0 -5"
  "cond fifo 1 999 9223372036854775807 0 0"
  "cond fifo 1 1 4611686018427387904 0 0"
  "cond fifo 1 1 1 0 0 foo"
  "cond fifo 1 1 1 0"
)
fallos=0
for c in "${casos[@]}"; do
  # shellcheck disable=SC2086
  salida=$(./rw $c 2>&1)
  codigo=$?
  if [ "$codigo" -ne 2 ]; then
    echo "FALLO (debió rechazarse, código $codigo): ./rw $c"
    fallos=$((fallos + 1))
  else
    echo "ok  ./rw $c  ->  $(echo "$salida" | head -1)"
  fi
done
echo "casos con fallo: $fallos"
[ "$fallos" -eq 0 ]
