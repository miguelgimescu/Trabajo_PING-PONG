# Análisis — Clasificador de Golpes de Ping Pong

## Descripción

Esta sección documenta el proceso de análisis de datos y entrenamiento del modelo de clasificación de golpes de ping pong. Se recogieron datos con el Arduino Nano 33 BLE Sense, se procesaron en Edge Impulse y se probaron múltiples configuraciones de red neuronal hasta encontrar la óptima.

---

## Datos recogidos

Los datos se capturaron directamente desde Edge Impulse con el Arduino Nano 33 BLE Sense conectado por USB, usando el Data Forwarder oficial de Edge Impulse, grbando 0 segundos de golpes y descartando las grabaciones con pocos y las que quedaron se les hizo poteriormente un recorte a los golpes en ventanas de 1 segundo.

| Parámetro | Valor |
|---|---|
| Sensores usados | accX, accY, accZ, gyrX, gyrY, gyrZ, magX, magY, magZ (9 ejes) |
| Frecuencia de muestreo | 100 Hz |
| Longitud de ventana | 1000 ms |
| Stride (solapamiento) | 500 ms |
| Total datos de entrenamiento | 14 min 12 s |
| Ventanas de entrenamiento | 852 |
| Clases | cortado, derecha, mate, revés, saque |
| Split train/validación | 80% / 20% |

Los datos crudos exportados desde Edge Impulse están disponibles en la carpeta `ping-pong-export (4).zip/`.

---

## Pipeline de procesado en Edge Impulse

```
Datos crudos (9 ejes IMU)
         │
         ▼
Spectral Analysis (DSP block)
→ Extrae características frecuenciales de cada eje
→ Genera 117 features por ventana
         │
         ▼
Red Neuronal Densa (Classification block)
→ Entrada: 117 features
→ Salida: 5 clases
```

El bloque **Spectral Analysis** transforma la señal temporal en características del dominio frecuencial (RMS, potencia espectral, frecuencia dominante) que son más discriminativas para gestos deportivos que la señal cruda.

---

## Algoritmos probados y limitaciones

### Limitación de la plataforma

Edge Impulse en su plan gratuito limita los bloques de aprendizaje disponibles. Algoritmos clásicos como **Random Forest** o **SVM**, que la literatura científica (ver manuscrito del Estado de la Cuestión) identifica como candidatos óptimos para inferencia embebida, solo están disponibles en el plan Enterprise de pago.

Por este motivo, el análisis se centró en **redes neuronales densas cuantizadas (int8)**, que sí están disponibles en el plan gratuito y son compatibles con el Arduino Nano 33 BLE Sense.

### Parámetros variables explorados

Se variaron sistemáticamente:
- **Número de capas densas:** 2 capas → 3 capas → 4 capas
- **Número de neuronas por capa:** desde 10 hasta 32
- **Épocas de entrenamiento:** 30, 100, 150, 200
- **Técnicas de regularización:** Dropout (0.25)

---

## Resumen de todas las pruebas

| Prueba | Arquitectura | Épocas | Accuracy | Loss | F1 | Observación |
|---|---|---|---|---|---|---|
| 1 | 20-10 | 30 | 94.2% | 0.17 | 0.94 | Baseline — saque falla (78.4%) |
| 2 | 32-16 | 100 | 98.8% | 0.05 | 0.99 | Mejora notable al ampliar red |
| 3 | 20-20-10 | 100 | 98.8% | 0.03 | 0.99 | 3 capas mejoran la loss |
| 4 | 20-20-10 | 150 | 98.8% | 0.02 | 0.99 | Loss mínima histórica hasta ahora |
| 5 | 32-20-10 | 150 | 98.2% | 0.06 | 0.98 | Más neuronas → ligero sobreajuste |
| 6 | 32-20-10 | 100 | 98.2% | 0.06 | 0.98 | Igual que prueba 5 — arquitectura menos eficiente |
| 7 | 32-20-10 + Dropout(0.25) | 100 | 97.1% | 0.10 | 0.97 | Dropout penaliza en este dataset |
| **8** | **24-20-10** | **100** | **99.4%** | **0.02** | **0.99** | **ÓPTIMO — récord absoluto** |
| 9 | 24-20-10 | 200 | 99.4% | 0.02 | 0.99 | Sin mejora → converge a 100 épocas |
| 10 | 26-20-10 | 100 | 98.8% | 0.03 | 0.99 | Subir a 26 neuronas empeora |
| 11 | 24-24-12 | 100 | 98.8% | 0.04 | 0.99 | Aumentar profundidad empeora |
| 12 | 24-18-12 | 100 | 98.8% | 0.03 | 0.99 | Reducir segunda capa empeora |
| 13 | 24-18-5 | 100 | 63.7% | 0.77 | — | Capa final de 5 neuronas colapsa el modelo |
| 14 | 24-20-10-8 | 100 | 98.8% | 0.05 | 0.99 | Cuarta capa no aporta nada |

---

## Modelo final seleccionado (Prueba 8)

**Arquitectura: 24-20-10 — 100 épocas — Learning rate 0.0005**

```
Entrada (117 features)
      │
Dense (24 neuronas)
      │
Dense (20 neuronas)
      │
Dense (10 neuronas)
      │
Salida (5 clases)
```

### Métricas finales (validation set)

| Métrica | Valor |
|---|---|
| Accuracy | **99.4%** |
| Loss | **0.02** |
| F1 Score ponderado | **0.99** |
| Area bajo curva ROC | 1.00 |
| Precisión ponderada | 0.99 |
| Recall ponderado | 0.99 |

### Matriz de confusión

| | Cortado | Derecha | Mate | Revés | Saque |
|---|---|---|---|---|---|
| **Cortado** | 100% | 0% | 0% | 0% | 0% |
| **Derecha** | 0% | 100% | 0% | 0% | 0% |
| **Mate** | 0% | 0% | 100% | 0% | 0% |
| **Revés** | 0% | 0% | 0% | 100% | 0% |
| **Saque** | 0% | 2.7% | 0% | 0% | 97.3% |

---

## Conclusiones del análisis

### 1. El saque es la clase más difícil de clasificar

En todas las pruebas, el saque es el único golpe que no alcanza el 100% de accuracy. Su confusión principal es con la derecha (2.7% en el modelo final), lo que es consistente con la literatura: ambos golpes comparten dirección de movimiento y solo se diferencian en detalles del gesto previo al impacto. Esto también se observó en los estudios de la revisión sistemática (ver manuscrito).

### 2. Arquitectura óptima: ni demasiado simple ni demasiado compleja

Las pruebas demuestran una tendencia clara en forma de U invertida:
- **Demasiado simple** (prueba 1, arquitectura 20-10): accuracy insuficiente, el saque falla al 78.4%.
- **Demasiado compleja** (prueba 13, capa final de 5 neuronas; o pruebas 5-6 con 32 neuronas): sobreajuste o colapso del modelo.
- **Punto óptimo** (prueba 8, arquitectura 24-20-10): equilibrio entre capacidad expresiva y generalización.

### 3. El Dropout penaliza en este dataset

La prueba 7 con Dropout(0.25) reduce el accuracy al 97.1%. El dataset es suficientemente grande y consistente como para que la regularización no sea necesaria, y el Dropout elimina información relevante durante el entrenamiento.

### 4. La convergencia se alcanza a las 100 épocas

La prueba 9 (200 épocas) produce exactamente los mismos resultados que la prueba 8 (100 épocas). Aumentar el tiempo de entrenamiento más allá de 100 épocas no aporta ningún beneficio con esta arquitectura y dataset.

### 5. Random Forest no disponible en plan gratuito

La revisión sistemática del proyecto identifica Random Forest como el algoritmo preferente para inferencia embebida en hardware de bajo coste. Sin embargo, Edge Impulse solo ofrece este clasificador en su plan Enterprise. La red neuronal densa cuantizada (int8) es la mejor alternativa disponible en el plan gratuito y alcanza resultados superiores (99.4%) a los reportados en la literatura para configuraciones equivalentes.

---

## Gráficas representativas

Las gráficas más relevantes del análisis están en la carpeta `capturas_edge/`:

Tambien esta la base de datos del entrenamiento subida a la carpeta.

El documento LaTeX completo con todas las matrices de confusión y gráficas de cada prueba está disponible en `analisis_Neural_Network_settings.pdf`.
