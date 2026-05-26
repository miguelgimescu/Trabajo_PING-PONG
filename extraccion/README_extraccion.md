# Extracción de Datos — Sistema Inalámbrico BLE

## Descripción

Dado que el proyecto exige un dispositivo completamente autónomo e inalámbrico (alimentado por batería y acoplado a la pala), el uso de cables USB para la toma de muestras estaba descartado, ya que alteraría la ergonomía y la cinemática real del golpe.

Para construir el dataset de entrenamiento, se desarrolló una arquitectura de extracción **100% inalámbrica** mediante Bluetooth Low Energy (BLE) y Python.

---

## Diagrama de funcionamiento

```
┌────────────────────────────┐      Bluetooth LE (BLE)      ┌────────────────────────┐
│ PALA (Batería externa)     │ ───────────────────────────► │ PC (Python + Bleak)    │
│                            │    Notificaciones GATT       │                        │
│ Arduino Nano 33 BLE Sense  │    "0.12,-0.34,1.01,..."     │ Recepción asíncrona    │
│ Muestreo IMU 100Hz         │ ───────────────────────────► │ Generación de .csv     │
│ LED verde = grabando       │                              │ Timestamping en Python │
│ LED azul  = esperando      │                              │                        │
└────────────────────────────┘                              └────────────────────────┘
         ▲                                                           │
    Sin cables                                                       ▼
  Libertad total                                           golpe_derecha_01.csv
```

---

## Archivos

| Archivo | Descripción |
|---|---|
| `extraccion_ble.ino` | Firmware Arduino: muestrea IMU y envía datos por BLE |
| `captura_dataset_ble.py` | Script Python: recibe datos BLE y guarda CSV |
| `datos_crudos/` | CSVs capturados con este método |

---

## Uso

1. Subir `extraccion_ble.ino` al Arduino y conectar la batería externa
2. Esperar a que el **LED se ponga azul** (esperando conexión)
3. Ajustar en `captura_dataset_ble.py` el nombre del archivo de salida según la clase a grabar (`golpe_derecha_01.csv`, `golpe_reves_01.csv`...)
4. Ejecutar el script — buscará automáticamente el dispositivo `PingPong_Data`
5. Cuando el **LED del Arduino se ponga verde**, la conexión está activa y está grabando
6. Realizar los golpes durante los 60 segundos de grabación
7. El CSV generado es compatible con Edge Impulse (`Data Acquisition → Upload existing data`)

---

## ⚙️ Retos de ingeniería superados

Transmitir 6 grados de libertad (acelerómetro + giroscopio en coma flotante) a 100Hz por BLE supuso el mayor reto técnico de esta fase, por las limitaciones de ancho de banda del protocolo.

**Optimización del payload:** Se eliminaron las marcas temporales en el envío del Arduino y se minimizaron los decimales, reduciendo el paquete a menos de 35 bytes para evitar pérdida de paquetes (*packet loss*).

**Reconstrucción temporal (timestamping):** El timestamp no lo genera el Arduino sino el script Python en el momento exacto de recepción asíncrona, manteniendo la fidelidad de la serie temporal necesaria para el bloque Spectral Analysis de Edge Impulse (FFT).

**Feedback visual sin cable:** El LED RGB del Arduino indica el estado del sistema sin necesidad de monitor serie: azul = esperando conexión, verde = grabando activamente.

---

## ⚠️ Evolución de la captura del dataset

Aunque este sistema funciona correctamente, durante el desarrollo del proyecto se comprobó que construir un dataset robusto es un proceso iterativo que consume varias horas de pruebas, generación de datos y descarte de muestras erróneas. Por este motivo, para la captura final se optó por utilizar la conexión directa mediante **Edge Impulse Data Forwarder**, empleando un cable USB suficientemente largo para mover la pala con libertad. La elección se basó en los siguientes criterios de eficiencia:

**1. Feedback visual inmediato (control de calidad)**
Capturar datos por BLE a ciegas obliga a revisar las gráficas a posteriori para detectar golpes anómalos. La herramienta nativa de Edge Impulse permite visualizar la onda en pantalla en tiempo real, lo que fue vital para descartar tomas defectuosas al instante durante las largas sesiones de captura.

**2. Gestión del etiquetado (labeling)**
El script Python requiere modificar el nombre del archivo de salida manualmente en cada nueva tanda de golpes. Edge Impulse centralizó este flujo, permitiendo grabar ráfagas continuas de movimiento y etiquetar los impactos directamente en la interfaz web, ahorrando un tiempo considerable.

Por tanto, el código adjunto sirve como prueba de concepto de la extracción raw inalámbrica y permite reproducir la captura de datos de forma autónoma sin depender de plataformas de terceros, pero el dataset final del proyecto fue generado con el método nativo para maximizar la eficiencia en el proceso de filtrado y etiquetado.

---

## Dependencias Python

```bash
pip install bleak
```
