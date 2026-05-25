"""
captura_dataset.py
Captura datos del Arduino Nano 33 BLE Sense por puerto serie
y los guarda en un archivo CSV compatible con Edge Impulse.

Uso:
  1. Subir extraccion_raw.ino al Arduino
  2. Cerrar el monitor serie de Arduino IDE
  3. Ajustar PUERTO y ARCHIVO_SALIDA
  4. Ejecutar: python captura_dataset.py
  5. Realizar el golpe durante los TIEMPO_CAPTURA_SEG segundos
"""

import serial
import time
import csv
import os

# ── Configuración ──────────────────────────────────────────────────────────────
PUERTO           = 'COM3'        # Windows: 'COM3', 'COM4'...
                                  # Linux/Mac: '/dev/ttyACM0', '/dev/cu.usbmodem...'
BAUDIOS          = 115200
TIEMPO_CAPTURA_SEG = 60          # Segundos de grabación por tanda
ARCHIVO_SALIDA   = "golpe_derecha_01.csv"  # Cambiar por clase y número
# ──────────────────────────────────────────────────────────────────────────────

def main():
    # Abrir puerto serie
    try:
        ser = serial.Serial(PUERTO, BAUDIOS, timeout=1)
        print(f"✓ Conectado a {PUERTO} a {BAUDIOS} baud")
    except Exception as e:
        print(f"✗ Error al abrir el puerto {PUERTO}: {e}")
        print("  Comprueba que el Arduino está conectado y el puerto es correcto.")
        return

    print(f"  Grabando durante {TIEMPO_CAPTURA_SEG} segundos...")
    print(f"  Archivo de salida: {ARCHIVO_SALIDA}")
    print(f"  Realiza los golpes ahora.\n")

    datos = []

    # Limpiar buffer inicial y descartar cabecera
    ser.reset_input_buffer()
    time.sleep(0.5)
    ser.readline()  # Descartar cabecera "timestamp,accX,..."

    tiempo_inicio = time.time()
    muestras = 0

    try:
        while (time.time() - tiempo_inicio) < TIEMPO_CAPTURA_SEG:
            if ser.in_waiting > 0:
                try:
                    linea = ser.readline().decode('utf-8').strip()
                    valores = linea.split(',')
                    if len(valores) == 7:
                        datos.append(valores)
                        muestras += 1
                        # Progreso cada segundo aproximadamente
                        if muestras % 100 == 0:
                            elapsed = time.time() - tiempo_inicio
                            print(f"  t={elapsed:.1f}s — {muestras} muestras capturadas", end='\r')
                except UnicodeDecodeError:
                    pass  # Ignorar bytes corruptos al inicio

    except KeyboardInterrupt:
        print("\n  Captura interrumpida por el usuario.")

    ser.close()

    tiempo_total = time.time() - tiempo_inicio
    freq_real = muestras / tiempo_total if tiempo_total > 0 else 0
    print(f"\n✓ Captura finalizada.")
    print(f"  Muestras: {muestras}")
    print(f"  Duración: {tiempo_total:.1f}s")
    print(f"  Frecuencia real: {freq_real:.1f} Hz (objetivo: 100 Hz)")

    if freq_real < 90 or freq_real > 110:
        print(f"  ⚠ ADVERTENCIA: La frecuencia real ({freq_real:.1f} Hz) se aleja de 100 Hz.")
        print(f"    Esto introduce jitter y puede reducir la precisión del modelo.")

    # Guardar CSV
    with open(ARCHIVO_SALIDA, mode='w', newline='') as archivo_csv:
        escritor = csv.writer(archivo_csv, delimiter=',')
        escritor.writerow(["timestamp", "accX", "accY", "accZ", "gyrX", "gyrY", "gyrZ"])
        escritor.writerows(datos)

    print(f"✓ Datos guardados en: {os.path.abspath(ARCHIVO_SALIDA)}")
    print(f"  Compatible con Edge Impulse (Data Acquisition → Upload existing data)")

if __name__ == "__main__":
    main()
