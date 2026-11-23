# Tarea 2 — Lectores y escritores

Sistemas Operativos 2026. Políticas de prioridad con un monitor (mutex + variables de condición) y con operaciones atómicas y CAS.

## Requisitos

Linux (o WSL en Windows) con **g++ (C++17)**, **make** y **bash**. Probado con g++ 15.2 en Ubuntu (WSL2).
Los scripts usan además `awk` y `column` (paquetes estándar de Ubuntu).

## Compilar

```bash
make                      # genera ./rw
make rw_tsan              # versión con ThreadSanitizer (-fsanitize=thread)
make prueba_verificador   # programa de prueba separado del verificador de traza
make clean
```

## Ejecutar

```bash
./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]
```

| Parámetro | Valores |
|---|---|
| `modo` | `cond`: monitor `MonitorRW` (mutex + variables de condición), 3 políticas<br>`atomic`: `ControlAtomicoRW` (atómicos + CAS), solo política `escritores` |
| `politica` | `lectores` \| `escritores` \| `fifo` |
| `L`, `E` | lectoras y escritoras (`L >= 1`, `E >= 1`, `L + E <= 1000`) |
| `K` | operaciones por hebra (`K >= 1`) |
| `dur_lec_us`, `dur_esc_us` | duración simulada de cada lectura/escritura en microsegundos (`>= 0`) |
| `notraza` | opcional: desactiva la traza de admisión (para las mediciones de E2) |

Ejemplos:

```bash
./rw cond escritores 6 2 100 200 500
./rw atomic escritores 6 2 100 200 500
./rw cond escritores 6 2 100 200 500 notraza
./rw cond fifo 8 1 200 1000 100
```

**Código de salida:** `0` si todo es correcto, `1` si falla una invariante o la política, `2` si los parámetros son inválidos o la combinación no está implementada.

**Variable de entorno opcional:** `RW_VOLCAR_TRAZA=<archivo>` escribe la traza completa (modo `cond`, con traza) en ese archivo, una anotación por línea.

## Salida

Resumen en líneas `clave=valor`: modo, política, operaciones, `version_final` y `doble_version_final` (con el valor esperado `EK` y `2EK`), `inconsistencias`, `violaciones_exclusion`, `max_lectoras_simultaneas`, `tiempo_pared_ms`, `tiempo_creacion_hebras_ms`, `tiempo_cpu_ms` (`CLOCK_PROCESS_CPUTIME_ID`), espera promedio/máxima de lectoras y escritoras, y `correcto=SI/NO`.

- **Modo `cond` con traza:** `anotaciones_traza`, violaciones encontradas (número de anotación y regla), `politica_ok=SI/NO` y `evidencia=N`.
- **Modo `atomic`:** `palabra_final` (debe ser 0) e intentos de admisión fallidos por causa (`recurso_ocupado`, `escritoras_esperando`, `cas_fallido`) para lectoras y escritoras.

## Pruebas

```bash
make pruebas    # trazas sintéticas del verificador + parámetros inválidos
```

## Reproducir los experimentos

| Comando | Qué hace | Resultados |
|---|---|---|
| `make e1` | E1: 3 políticas `cond` + `atomic escritores`, 5 configuraciones, 5 veces | `resultados/e1/e1.csv`, `resultados/e1/resumen_e1.txt`, `resultados/e1/salidas/*.txt` |
| `make e2` | E2: escritores `cond` vs `atomic`, `notraza`, 5 veces y promedios (la configuración sin pausas usa `K = 10000`) | `resultados/e2/e2.csv`, `resultados/e2/promedios_e2.txt` |
| `make fragmentos` | Fragmentos de traza por política para el anexo | `resultados/trazas/fragmento_<politica>.txt` |

El número de repeticiones se puede cambiar con `REPS`, por ejemplo:

```bash
REPS=10 bash experimentos_e2.sh
```

## Búsqueda de carreras de datos

```bash
make rw_tsan
./rw_tsan cond fifo 8 8 300 0 0
./rw_tsan atomic escritores 8 8 300 0 0
```

ThreadSanitizer solo detecta carreras en los entrelazamientos que efectivamente ocurren; la ausencia de alertas no prueba que no existan.

## En Windows (WSL)

Desde PowerShell, en la carpeta del proyecto:

```powershell
wsl make
wsl ./rw cond escritores 6 2 100 200 500
```

## Archivos

| Archivo | Contenido |
|---|---|
| `comun.h` | `ControlRW`, `Politica`, `Registro`, `AnotacionTraza`, `ContadoresFallos` |
| `monitor_rw.h` / `.cpp` | Parte I: `MonitorRW` (lectores, escritores, fifo) |
| `control_atomico_rw.h` / `.cpp` | Parte II: `ControlAtomicoRW` (palabra de 64 bits, CAS) |
| `verificador.h` / `.cpp` | `verificar_traza` (R6) |
| `rw.cpp` | `main`: parámetros, hebras, registro, invariantes, medición |
| `prueba_verificador.cpp` | Pruebas del verificador con trazas construidas a mano |
| `pruebas_parametros.sh` | Pruebas de parámetros inválidos |
| `experimentos_e1.sh`, `experimentos_e2.sh`, `fragmentos_traza.sh` | Experimentos |
