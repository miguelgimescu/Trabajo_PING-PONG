# Trabajo Práctico: Clasificador de Golpes de Ping Pong con Redes de Sensores

> Sistema de detección y clasificación en tiempo real de golpes de ping pong usando Arduino Nano 33 BLE Sense, análisis espectral y redes neuronales embebidas.

**Autor:** Miguel Gimescu  
**Asignatura:** Redes de Sensores  
**Año:** 2026

---

## 🎯 Objetivo Principal

Desarrollar un clasificador automático de golpes de ping pong que funcione en tiempo real en un microcontrolador de bajo coste, sin dependencias de cloud, combinando captura de datos, análisis con machine learning y ejecución embebida.

**Clases detectadas:** Saque, Derecha, Revés, Cortado y Mate.

---

## 📊 Resultados Destacados

| Métrica | Valor |
|---------|-------|
| **Accuracy en validación** | 99.4% |
| **Modelo seleccionado** | Red neuronal 24-20-10 (3 capas) |
| **Características extractadas** | 117 features por ventana (DSP/Spectral) |
| **Latencia inferencia** | 5-10 ms en Arduino Nano |
| **Tamaño modelo (cuantizado)** | ~15 KB |
| **Arquitecturas probadas** | 14 diferentes |

---

## 📁 Estructura del Proyecto

El proyecto se organiza en **4 fases principales**, cada una en su carpeta con documentación independiente:

### 1. **[📖 Introducción](/introduccion/README.md)** 
   Especificación del proyecto, objetivos, contexto de redes de sensores y referencias bibliográficas.
   
   📌 **Incluye:**
   - Descripción general del sistema
   - Contexto de IoT y computación embebida
   - 5 objetivos específicos desglosados
   - Arquitectura ASCII visual
   - Referencias al documento de proyecto anterior

---

### 2. **[📥 Extracción](/extraccion/README_extraccion.md)**
   Código y documentación para capturar datos crudos desde el sensor.
   
   📌 **Incluye:**
   - Firmware Arduino que muestrea IMU a 100 Hz
   - Script Python que recibe datos por puerto serie
   - Diagrama de flujo de captura
   - Formato de datos CSV generado
   - Dataset de ejemplo

   **Archivos:**
   - `extraccion_raw.ino` — Firmware Arduino
   - `captura_dataset.py` — Script Python de captura
   - `ping-pong-export (4).zip` — Dataset capturado

---

### 3. **[📊 Análisis](/analisis/README_analisis.md)**
   Análisis comparativo de 14 modelos de red neuronal probados en Edge Impulse.
   
   📌 **Incluye:**
   - Datos brutos del dataset (852 muestras)
   - Pipeline de procesado (Spectral Analysis → 117 features)
   - Tabla comparativa de 14 arquitecturas
   - **Modelo ganador: 24-20-10 con 99.4% accuracy**
   - Matriz de confusión detallada
   - Conclusiones del análisis
   - Gráficas y visualizaciones

   **Archivos:**
   - `capturas_edge/` — 14 screenshots de Edge Impulse
   - `analisis_Neural_Network_settings.pdf` — Documento técnico con todas las métricas
   - `ping-pong-export (4).zip` — Datos de entrenamiento

---

### 4. **[⚙️ Implementación](/implementacion/README_implementacion.md)**
   Código Arduino final que ejecuta el modelo en tiempo real con máquina de estados.
   
   📌 **Incluye:**
   - Sistema de detección de picos (magnitud aceleración > 14 m/s²)
   - Buffer circular de 1000ms con pre/post-pico
   - Máquina de estados (ESPERANDO → POST_PICO → CLASIFICANDO)
   - Comunicación BLE bidireccional (notificaciones + control remoto)
   - Feedback visual con LED RGB
   - Parámetros ajustables (umbrales, cooldown, etc.)
   - Guía de uso con nRF Connect

   **Archivos:**
   - `nano_ble33_sense_fusion_ping_pong/` — Proyecto Arduino compilado
   - `ei-ping-pong-arduino-1.0.1-impulse-#1.zip` — Librería exportada de Edge Impulse

---

## 🔄 Flujo de Trabajo Completo

```
┌─────────────────────────────────────────────────────────────┐
│                    EXTRACCIÓN                               │
│  Arduino muestrea IMU 100Hz → CSV → Edge Impulse           │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                      ANÁLISIS                               │
│  Spectral Analysis (DSP) → 117 features                     │
│  Prueba 14 arquitecturas → Modelo óptimo 24-20-10           │
│  Accuracy: 99.4% — Matriz confusión perfecta               │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                   IMPLEMENTACIÓN                            │
│  Modelo cuantizado (int8) en Arduino                        │
│  Máquina de estados + detección de picos                    │
│  BLE para supervisión remota                               │
│  LED RGB para feedback visual                              │
└─────────────────────────────────────────────────────────────┘
                             ↓
                    🎾 EN TIEMPO REAL
```

---

## 🛠 Tecnologías Principales

| Capa | Tecnología |
|-----|-----------|
| **Hardware** | Arduino Nano 33 BLE Sense + IMU LSM9DS1 (6 ejes) |
| **Sensores** | Acelerómetro + Giroscopio @ 100 Hz |
| **ML Platform** | Edge Impulse (DSP + Neural Networks) |
| **Modelo** | Red neuronal densa cuantizada (int8) |
| **Firmware** | C++ (Arduino IDE) |
| **Comunicación** | Bluetooth Low Energy (BLE) |
| **Scripts** | Python 3.x |

---

## 📖 Documentación Relacionada

- **Documento de proyecto anterior (referencia):** [REDES_DE_SENSORES_Trabajo.pdf](https://github.com/miguelgimescu/Proyecto-redes-de-sensores/blob/main/REDES_DE_SENSORES_Trabajo.pdf)
- **Edge Impulse Docs:** https://docs.edgeimpulse.com/
- **Arduino Nano 33 BLE Sense:** https://docs.arduino.cc/hardware/nano-33-ble-sense/

---

## 🚀 Quick Start

1. **Ver introducción:** Abre [`/introduccion/README.md`](/introduccion/README.md)
2. **Entender captura:** Lee [`/extraccion/README_extraccion.md`](/extraccion/README_extraccion.md)
3. **Analizar resultados:** Consulta [`/analisis/README_analisis.md`](/analisis/README_analisis.md)
4. **Usar el sistema:** Sigue [`/implementacion/README_implementacion.md`](/implementacion/README_implementacion.md)

---

## 📝 Notas

Este proyecto demuestra la aplicación práctica de **redes de sensores** combinando:
- ✅ Adquisición de datos en tiempo real (IoT)
- ✅ Análisis de señales (DSP)
- ✅ Machine Learning (entrenamiento sistemático)
- ✅ Computación embebida (sin cloud)
- ✅ Comunicación inalámbrica (BLE)
- ✅ Interfaz remota (supervisión móvil)

---

**Año:** 2026 | **Asignatura:** Redes de Sensores | **Autor:** Miguel Gimescu
