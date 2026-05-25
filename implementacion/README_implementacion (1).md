# Implementación Final — Clasificador de Golpes de Ping Pong

## Descripción

Sketch de Arduino para el Arduino Nano 33 BLE Sense que realiza clasificación en tiempo real de golpes de ping pong usando un modelo de red neuronal exportado desde Edge Impulse, con comunicación inalámbrica BLE para supervisión y control remoto desde el móvil.

Las clases detectadas son: **saque, derecha, revés, cortado y mate**.

---

## Diagrama de funcionamiento

```
┌─────────────────────────────────────────────────────────────┐
│                       LOOP 100Hz                            │
│                                                             │
│  IMU (acc+gyro) ──► Buffer circular 1000ms                  │
│                              │                              │
│                       Calcular magnitud                     │
│                       aceleración actual                    │
│                              │                              │
│                    BLE.poll() ◄──── móvil (nRF Connect)     │
│                    controlChar?                             │
│                    0 = PAUSAR / 1 = REANUDAR                │
│                              │                              │
│              ┌───────────────▼──────────────┐              │
│              │   MÁQUINA DE ESTADOS         │              │
│              │   (solo si sistema_activo)   │              │
│              │                              │              │
│   ESPERANDO ─► magnitud > 14 m/s²?         │              │
│              │         │ SÍ                 │              │
│              │   POST_PICO                  │              │
│              │   esperar 60 muestras        │              │
│              │   (600ms post-pico)          │              │
│              │         │                    │              │
│              │   CLASIFICANDO               │              │
│              └───────────────┬──────────────┘              │
│                              │                             │
│              Ventana 1000ms al modelo Edge Impulse          │
│              [200ms pre-pico + 600ms post-pico]            │
│                              │                             │
│                   confianza ≥ 0.92?                        │
│                  │SÍ                  │NO                  │
│           Imprimir Serial         Imprimir [descartado]    │
│           + LED color             + BLE descarteChar       │
│           + BLE golpeChar                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## Decisiones de diseño y por qué llegamos a ellas

### 1. Estrategia de ventana: detección de pico + pre/post buffer

**El problema:** el modelo fue entrenado en Edge Impulse con ventanas de 1000ms recortadas automáticamente alrededor de cada golpe. En tiempo real, una ventana deslizante ciega clasifica cada N ms sin saber si hay golpe o no, produciendo clasificaciones erróneas continuamente.

**La solución:** implementar una máquina de estados que detecta el momento exacto del golpe (pico de aceleración) y construye la ventana a partir de ese punto:
- **20 muestras antes del pico (200ms):** captura el inicio del movimiento y la rampa de subida, que contiene información diferenciadora entre clases.
- **60 muestras después del pico (600ms):** captura el follow-through del golpe.
- **Total: 80 muestras = 800ms** — este balance (200+600) produjo los mejores resultados experimentalmente.

**Por qué 200ms antes y 600ms después:** se probaron varias combinaciones (400+600, 50+950, 5+95) y la combinación 20+60 muestras fue la que mejor separó las clases. Con más pre-pico el modelo confundía derecha y saque con mate; con menos pre-pico perdía el inicio del movimiento.

---

### 2. Umbral de detección de pico: 14.0 m/s²

**El problema:** el sensor en reposo da ~9.8 m/s² por la gravedad. Los golpes llegan hasta ~19-20 m/s² (limitado por MAX_ACCEPTED_RANGE = 2G = 19.6 m/s²).

**Por qué 14.0:** deja margen suficiente sobre el reposo (9.8) sin requerir golpes muy fuertes. Valores más altos (150 m/s²) no detectaban nada porque el acelerómetro está limitado a 2G por el firmware de Edge Impulse. Valores más bajos generaban falsas detecciones con movimientos suaves.

**Nota:** MAX_ACCEPTED_RANGE = 2.0f limita todos los valores del acelerómetro a ±2G = ±19.6 m/s², coherente con cómo Edge Impulse capturó los datos de entrenamiento.

---

### 3. Umbral de confianza: 0.92

**El problema inicial:** con umbral 0.8 el modelo clasificaba continuamente aunque la pala estuviese en reposo.

**Por qué 0.92:** el modelo alcanzó 99.4% de accuracy en validación, por lo que es seguro exigir alta confianza sin perder detecciones reales. Cuando la confianza no llega a 0.92 el resultado se imprime como `[descartado]` por Serial y se notifica también por BLE via `descarteChar` para facilitar el debug remoto.

---

### 4. Cooldown de 1200ms entre detecciones

**El problema:** un golpe dura ~400-500ms y el sistema podría detectarlo varias veces consecutivas.

**Por qué 1200ms:** tiempo suficiente para que el golpe termine completamente y el jugador vuelva a posición de espera antes de la siguiente detección, evitando duplicados sin perder golpes reales en un rally.

---

### 5. Sin hilo de inferencia (sin rtos::Thread)

El ejemplo original de Edge Impulse usa `rtos::Thread` para correr la inferencia en background, pero esta librería no está disponible en todas las versiones del core de Arduino Nano 33 BLE Sense y genera error de compilación. Todo corre en el loop principal sin perder el timing de 100Hz porque la clasificación solo se lanza una vez por golpe detectado.

---

### 6. Comunicación BLE — por qué y cómo

**Por qué BLE:** permite supervisar y controlar el sistema sin cable, lo que es especialmente útil cuando el sensor está fijado en la pala durante el juego. Se usa la librería `ArduinoBLE` nativa del Arduino Nano 33 BLE Sense.

Se han definido tres características dentro de un servicio personalizado:

| Característica | UUID final | Tipo | Función |
|---|---|---|---|
| `golpeChar` | `...1214` | BLERead + BLENotify | Envía golpe detectado y confianza |
| `descarteChar` | `...1215` | BLERead + BLENotify | Envía movimientos descartados por baja confianza |
| `controlChar` | `...1216` | BLERead + BLEWrite | Recibe 0 (pausar) o 1 (reanudar) desde el móvil |

**Por qué tres características separadas:** separa los canales de información para que la app móvil pueda suscribirse selectivamente. Los descartes en su propio canal evitan contaminar el canal de golpes válidos y son útiles para debug sin interferir con el flujo normal.

---

### 7. Sistema de control remoto (interruptor 0/1)

Cuando `controlChar` recibe un `0` desde el móvil, `sistema_activo = false` desactiva la máquina de estados pero el loop sigue corriendo y leyendo el IMU. Esto mantiene el buffer circular actualizado y permite reanudar instantáneamente sin perder muestras ni reiniciar el buffer.

---

### 8. LED RGB como feedback visual

El LED integrado (cátodo común, LOW = encendido) muestra el golpe durante 800ms:

| Golpe | Color LED |
|---|---|
| Saque | Azul |
| Derecha | Verde |
| Revés | Amarillo (R+G) |
| Cortado | Rojo |
| Mate | Blanco (R+G+B) |
| No golpe / descartado | Apagado |

---

### 9. Sensor: fusión acelerómetro + giroscopio (6 ejes)

Se usa el bloque `fusion` de Edge Impulse (accX, accY, accZ, gyrX, gyrY, gyrZ) porque el giroscopio es clave para distinguir golpes con patrones de aceleración similares pero distinta velocidad angular, especialmente derecha vs cortado.

---

## Output por consola (115200 baud)

```
=== Clasificador Ping Pong ===
Umbral pico deteccion: 14.00 m/s2
Umbral confianza:      0.92
Pre-pico:  200ms
Post-pico: 600ms
Esperando golpes...
BLE Activo. Emitiendo advertising como 'NanoPingPong'...

>> GOLPE: mate     (98.1%)
>> GOLPE: reves    (95.3%)
>> GOLPE: cortado  (93.7%)
   [descartado: derecha 71.2%]
>> Sistema PAUSADO desde el movil
>> Sistema REANUDADO desde el movil
```

---

## Guía de uso con nRF Connect

Conecta el móvil mediante la app **nRF Connect** y busca el dispositivo `NanoPingPong`:

1. **Característica de Golpes** (`...1214`): activa las notificaciones (icono de la flecha hacia abajo) para recibir en tiempo real el nombre del golpe y su confianza. Ejemplo: `mate (98.1%)`
2. **Característica de Descartes** (`...1215`): notificaciones de movimientos rechazados por baja confianza. Ejemplo: `X derecha (71.2%)`
3. **Característica de Control** (`...1216`):
   - Usa el icono de escritura (flecha hacia arriba)
   - Envía `0` (formato Byte) → **Pausar** clasificación
   - Envía `1` (formato Byte) → **Reanudar** clasificación

---

## Parámetros ajustables

```cpp
#define UMBRAL_PICO          14.0f   // Subir si hay falsas detecciones en reposo
#define UMBRAL_CONFIANZA     0.92f   // Bajar si no detecta golpes válidos
#define MUESTRAS_PRE_PICO    20      // Más = más contexto antes del golpe
#define MUESTRAS_POST_PICO   60      // Más = más follow-through capturado
#define COOLDOWN_MS          1200    // Aumentar si detecta el mismo golpe dos veces
```

---

## Dependencias

- Librería exportada desde Edge Impulse (`PING-PONG_inferencing.h`)
- `ArduinoBLE` — comunicación Bluetooth Low Energy
- `Arduino_LSM9DS1` — IMU acelerómetro y giroscopio
- `Arduino_LPS22HB` — barómetro (requerido por bloque fusion)
- `Arduino_HTS221` — temperatura y humedad (requerido por bloque fusion)
- `Arduino_APDS9960` — sensor de color y proximidad (requerido por bloque fusion)
