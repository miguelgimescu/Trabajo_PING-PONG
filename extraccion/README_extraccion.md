# Extracción de Datos — Clasificador de Golpes de Ping Pong

## Descripción

Este directorio contiene los scripts iniciales desarrollados para la captura del dataset de movimientos de ping pong. El sistema consta de un firmware en Arduino que muestrea la IMU a 100Hz y un script en Python que recoge los datos vía UART y los almacena en formato `.csv` para su posterior procesamiento.

---

## Diagrama de funcionamiento

```
┌─────────────────┐     UART (115200 baud)      ┌─────────────────┐
│ Arduino Nano 33 │ ──────────────────────────► │ PC (Python)     │
│                 │                             │                 │
│ Muestreo IMU    │    "1250,0.5,1.2,9.8,..."   │ pyserial        │
│ a 100Hz (10ms)  │ ──────────────────────────► │ Guarda en .csv  │
└─────────────────┘                             └─────────────────┘
        ▲                                               │
        │ Captura de movimiento                         ▼
   Pala de Ping Pong                          golpe_derecha_01.csv
```

**Ejes capturados:** accX, accY, accZ, gyrX, gyrY, gyrZ a 100Hz

---

## Archivos

| Archivo | Descripción |
|---|---|
| `extraccion_raw.ino` | Firmware Arduino: muestrea IMU y envía CSV por puerto serie |
| `captura_dataset.py` | Script Python: recibe datos serie y los guarda en CSV |
| `datos_crudos/` | Carpeta con los CSV capturados con este método |

---

## Uso de los scripts

1. Subir `extraccion_raw.ino` al Arduino Nano 33 BLE Sense
2. Comprobar en el monitor serie que los datos salen con el formato:
   ```
   timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
   1250,0.12,-0.34,1.01,2.3,-1.2,0.5
   1260,0.13,-0.33,1.02,2.4,-1.1,0.4
   ```
3. Cerrar el monitor serie de Arduino IDE
4. Editar `captura_dataset.py` con el puerto COM correcto y el nombre del archivo de salida
5. Ejecutar el script y realizar el golpe durante los segundos configurados
6. El CSV generado es compatible con Excel y puede subirse manualmente a Edge Impulse

---

## ⚠️ Nota de implementación y evolución del proyecto

Aunque este sistema de extracción propio funciona correctamente, **se descartó para la captura final del dataset en favor de una conexión directa por cable usando el Edge Impulse Data Forwarder** por dos motivos técnicos:

**1. Jitter en la frecuencia de muestreo**

La latencia del sistema operativo al leer el puerto serie mediante Python introduce pequeñas fluctuaciones temporales (*jitter*). El bloque de procesado Spectral Analysis de Edge Impulse extrae características frecuenciales (FFT, potencia espectral) que requieren que las muestras estén alineadas exactamente a 100Hz. Con Python recibiendo por serie, las muestras llegaban con separaciones de entre 9ms y 12ms en vez de los 10ms exactos requeridos, lo que degrada la calidad de las features y por tanto la precisión del modelo.

**2. Fricción en el etiquetado manual**

Capturar las ~850 ventanas de entrenamiento necesarias (5 clases × ~170 ventanas) con este método implicaba:
- Ejecutar el script una vez por cada tanda de golpes
- Renombrar cada CSV manualmente (`derecha_01.csv`, `derecha_02.csv`...)
- Subirlos uno a uno a Edge Impulse
- Recortar manualmente cada golpe dentro del CSV

Usando el **Edge Impulse Data Forwarder con cable USB largo**, el flujo de trabajo fue radicalmente más eficiente: grabar 30 segundos de golpes continuos, etiquetar la clase en la web, y dejar que Edge Impulse segmentara automáticamente las ventanas. El tiempo de recogida de datos pasó de varias horas a aproximadamente 45 minutos.

Por tanto, el código adjunto sirve como prueba de concepto de la extracción raw y permite reproducir la captura de datos de forma autónoma sin depender de la plataforma Edge Impulse, pero el dataset final del proyecto fue generado con el método nativo para maximizar la fidelidad de las señales y la eficiencia del proceso.

---

## Dependencias Python

```bash
pip install pyserial
```

---

## Formato del CSV generado

```
timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
1250,0.12,-0.34,1.01,2.30,-1.20,0.50
1260,0.13,-0.33,1.02,2.40,-1.10,0.40
...
```

Compatible con Excel (abrir como CSV con delimitador coma) y con la opción de subida manual de Edge Impulse (`Data Acquisition → Upload existing data`).
