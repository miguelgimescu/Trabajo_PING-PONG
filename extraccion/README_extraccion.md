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

## ⚙️ Evolución de la Captura del Dataset

Aunque este sistema de extracción propio funciona correctamente, durante el desarrollo del proyecto se comprobó que la construcción de un dataset robusto es un proceso iterativo complejo que consume varias horas de pruebas, generación de datos y descarte de muestras erróneas. Por este motivo pragmático, para la captura final se optó por utilizar la conexión directa por cable mediante **Edge Impulse Data Forwarder**, basándonos en los siguientes criterios de eficiencia:

**1. Feedback Visual Inmediato (Control de Calidad)**

Capturar datos a ciegas en un CSV mediante Python obliga a revisar las gráficas a posteriori para detectar golpes anómalos o mal ejecutados. La herramienta nativa de Edge Impulse permite visualizar la onda en la pantalla del ordenador en tiempo real, lo que fue vital para descartar tomas defectuosas al instante durante las largas sesiones de captura.

**2. Gestión del Etiquetado (Labeling)**

El script de Python requiere modificar el código fuente manualmente para cambiar el nombre del archivo de salida en cada nueva tanda de golpes (por ejemplo, cambiar de `derecha_01.csv` a `reves_01.csv`). Edge Impulse centralizó este flujo, permitiendo grabar ráfagas continuas de movimiento y etiquetar los impactos de forma mucho más ágil directamente en la interfaz web, ahorrando un tiempo valiosísimo.

Por tanto, el código adjunto sirve como prueba de concepto de la extracción raw y permite reproducir la captura de datos de forma autónoma sin depender de plataformas de terceros, pero el dataset final del proyecto fue generado con el método nativo para maximizar la eficiencia en el proceso de filtrado y etiquetado manual.

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
