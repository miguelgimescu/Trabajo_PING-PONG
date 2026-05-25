# Introducción — Clasificador de Golpes de Ping Pong con Redes de Sensores

## Descripción del Proyecto

Este proyecto implementa un **sistema de clasificación en tiempo real de golpes de ping pong** utilizando un Arduino Nano 33 BLE Sense equipado con sensores de movimiento (IMU de 6 ejes). El sistema combina captura de datos, análisis con aprendizaje automático y ejecución embebida de un modelo neuronal en hardware de bajo consumo.

**Clases detectadas:** Saque, Derecha, Revés, Cortado y Mate.

---

## Contexto: Redes de Sensores

Este trabajo se enmarca en la asignatura de **Redes de Sensores**, donde el objetivo es aplicar principios de IoT y computación embebida para resolver problemas reales de detección y clasificación. 

El proyecto demuestra:
- **Adquisición de datos** desde sensores inerciales a baja latencia (100 Hz)
- **Comunicación inalámbrica** via Bluetooth Low Energy (BLE) para supervisión remota
- **Procesamiento local** de modelos de ML sin dependencias de cloud
- **Eficiencia energética** mediante cuantización de modelos (int8)

---

## Objetivos del Proyecto

### Objetivo Principal
Desarrollar un clasificador de golpes de ping pong que funcione en **tiempo real** sobre un microcontrolador ARM de bajo coste, sin conexión a servidores externos.

### Objetivos Específicos

1. **Captura de datos robusta**
   - Muestrear IMU (acelerómetro + giroscopio) a 100 Hz
   - Generar dataset balanceado con ~850 muestras por clase
   - Garantizar etiquetado consistente mediante herramientas de terceros (Edge Impulse)

2. **Análisis y extracción de características**
   - Aplicar análisis espectral (DSP) a las señales temporales
   - Extraer 117 características frecuenciales por ventana de 1000ms
   - Identificar qué características discriminan mejor cada clase

3. **Entrenamiento de modelo óptimo**
   - Evaluar sistemáticamente arquitecturas de redes neuronales densas
   - Encontrar el equilibrio entre precisión y complejidad (14 pruebas)
   - Lograr **99.4% de accuracy** en validación con arquitectura 24-20-10

4. **Implementación embebida**
   - Exportar modelo cuantizado (int8) desde Edge Impulse
   - Implementar máquina de estados para detección de picos de aceleración
   - Integrar feedback visual (LED RGB) y audible (BLE notifications)

5. **Supervisión remota**
   - Comunicación BLE bidireccional con aplicación móvil
   - Control remoto (pausar/reanudar clasificación)
   - Envío de resultados en tiempo real a dispositivos clientes

---

## Estudio de la Cuestión

Se revisó bibliografía sobre clasificación de gestos y acciones humanas en redes de sensores:

- **Random Forest** como algoritmo preferente para hardware embebido (bajo consumo, sin derivadas)
- **Redes neuronales cuantizadas** como alternativa viable cuando Random Forest no está disponible
- **Ventanas deslizantes adaptativas** basadas en detección de picos para mejorar latencia
- **Fusión sensorial** (acelerómetro + giroscopio) para mayor discriminabilidad

**Documento de referencia histórico:** [REDES_DE_SENSORES_Trabajo.pdf](https://github.com/miguelgimescu/Proyecto-redes-de-sensores/blob/main/REDES_DE_SENSORES_Trabajo.pdf) — Proyecto anterior que estableció la base conceptual para este trabajo.

---

## Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────────┐
│                   ARQUITECTURA GENERAL                       │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │          FASE 1: CAPTURA DE DATOS                     │ │
│  │  Arduino Nano 33 BLE + IMU (accX,Y,Z, gyrX,Y,Z)      │ │
│  │  100 Hz → CSV → Edge Impulse                          │ │
│  └────────────────────────────────────────────────────────┘ │
│                          ↓                                   │
│  ┌────────────────────────────────────────────────────────┐ │
│  │          FASE 2: ANÁLISIS Y ENTRENAMIENTO             │ │
│  │  Spectral Analysis (DSP) → 117 features               │ │
│  │  Redes Neuronales Densas (14 arquitecturas probadas)  │ │
│  │  Modelo óptimo: 24-20-10 (99.4% accuracy)            │ │
│  └────────────────────────────────────────────────────────┘ │
│                          ↓                                   │
│  ┌────────────────────────────────────────────────────────┐ │
│  │      FASE 3: IMPLEMENTACIÓN Y DESPLIEGUE              │ │
│  │  Modelo cuantizado (int8) → Arduino                   │ │
│  │  Máquina de estados + detección de picos             │ │
│  │  BLE para supervisión remota                          │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## Estructura del Repositorio

```
Trabajo_PING-PONG/
├── introduccion/              ← Tú estás aquí
│   └── README.md
├── extraccion/
│   ├── README_extraccion.md   ← Captura de datos raw
│   ├── extraccion_raw.ino
│   └── captura_dataset.py
├── analisis/
│   ├── README_analisis.md     ← Resultados de 14 modelos probados
│   ├── capturas_edge/         ← Screenshots de Edge Impulse
│   └── ping-pong-export.zip   ← Dataset y configuraciones
└── implementacion/
    ├── README_implementacion.md  ← Sistema final en tiempo real
    └── nano_ble33_sense_fusion_ping_pong/  ← Código Arduino
```

---

## Flujo de Trabajo

1. **Extracción** (Carpeta `/extraccion`)
   - Script Arduino muestrea IMU a 100 Hz
   - Script Python recibe datos serie y guarda en CSV
   - Datos se suben a Edge Impulse para etiquetado profesional

2. **Análisis** (Carpeta `/analisis`)
   - Edge Impulse aplica Spectral Analysis automática
   - Se entrenan 14 arquitecturas diferentes de redes neuronales
   - Se selecciona el modelo óptimo: **24-20-10 con 99.4% accuracy**

3. **Implementación** (Carpeta `/implementacion`)
   - Modelo cuantizado se exporta desde Edge Impulse
   - Arduino ejecuta máquina de estados que detecta golpes
   - LED RGB + BLE notifications informan resultados

---

## Tecnologías Utilizadas

| Componente | Tecnología |
|---|---|
| **Microcontrolador** | Arduino Nano 33 BLE Sense |
| **Sensores** | IMU LSM9DS1 (6 ejes: acelerómetro + giroscopio) |
| **Frecuencia muestreo** | 100 Hz |
| **Comunicación** | Bluetooth Low Energy (BLE) |
| **Machine Learning** | Edge Impulse (DSP + Neural Networks) |
| **Lenguajes** | C++ (Arduino), Python (captura), LaTeX (análisis) |

---

## Resultados Clave

- **Accuracy en validación:** 99.4%
- **Modelo seleccionado:** 3 capas densas (24-20-10 neuronas)
- **Tamaño del modelo:** ~15 KB (cuantizado int8)
- **Latencia inferencia:** ~5-10 ms en Arduino Nano
- **Consumo BLE:** ~10-20 mA en operación
- **Matriz de confusión:** Perfecta en 4 de 5 clases; saque ~97.3% (confusión con derecha 2.7%)

---

## Referencias

- **Documento de proyecto anterior:** [REDES_DE_SENSORES_Trabajo.pdf](https://github.com/miguelgimescu/Proyecto-redes-de-sensores/blob/main/REDES_DE_SENSORES_Trabajo.pdf)
- **Edge Impulse Documentation:** https://docs.edgeimpulse.com/
- **Arduino Nano 33 BLE Sense:** https://docs.arduino.cc/hardware/nano-33-ble-sense/
- **Sensor LSM9DS1:** https://github.com/arduino/ArduinoCore-nrf52/tree/master/libraries/Arduino_LSM9DS1

---

## Autor

Miguel Gimescu

**Asignatura:** Redes de Sensores  
**Año:** 2026
