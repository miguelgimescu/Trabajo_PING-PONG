"""
captura_dataset_ble.py
Recibe datos IMU del Arduino Nano 33 BLE Sense por Bluetooth Low Energy
y los guarda en un archivo CSV listo para Edge Impulse.

El Arduino va montado en la pala con batería externa — sin cables al PC.

Uso:
  1. Subir extraccion_ble.ino al Arduino y alimentarlo con batería externa
  2. Esperar a que el LED del Arduino se ponga azul (esperando conexion)
  3. Ajustar ADDRESS con la MAC del Arduino (ver nota abajo)
  4. Ajustar ARCHIVO_SALIDA con el nombre del golpe a grabar
  5. Ejecutar: python captura_dataset_ble.py
  6. Realizar los golpes durante TIEMPO_CAPTURA_SEG segundos

Nota MAC: Para encontrar la MAC del Arduino, usar nRF Connect en el móvil
o ejecutar este script con ADDRESS = None (modo escaneo automático).

Dependencias:
  pip install bleak
"""

import asyncio
from bleak import BleakClient, BleakScanner
import time
import csv
import os

# ── Configuración ──────────────────────────────────────────────────────────────
ADDRESS          = "XX:XX:XX:XX:XX:XX"  # MAC del Arduino (ver nota arriba)
UUID_IMU         = "19b10011-e8f2-537e-4f6c-d104768a1214"
TIEMPO_CAPTURA_SEG = 60                  # Segundos de grabación por tanda
ARCHIVO_SALIDA   = "golpe_derecha_01.csv"  # Cambiar por clase y número
# ──────────────────────────────────────────────────────────────────────────────

datos        = []
tiempo_inicio = 0
paquetes_ok   = 0
paquetes_err  = 0

def recepcion_datos(sender, data):
    """Callback BLE — se ejecuta en cada notificación recibida."""
    global paquetes_ok, paquetes_err
    try:
        texto   = data.decode('utf-8')
        valores = texto.split(',')
        if len(valores) == 6:
            # El timestamp lo genera Python en recepcion
            # para evitar deriva del reloj del Arduino sin USB
            timestamp = int((time.time() - tiempo_inicio) * 1000)
            datos.append([timestamp] + valores)
            paquetes_ok += 1
        else:
            paquetes_err += 1
    except Exception:
        paquetes_err += 1

async def escanear_arduino():
    """Busca automaticamente el dispositivo PingPong_Data."""
    print("Escaneando dispositivos BLE...")
    devices = await BleakScanner.discover(timeout=5.0)
    for d in devices:
        if d.name and "PingPong" in d.name:
            print(f"  Encontrado: {d.name} — {d.address}")
            return d.address
    return None

async def run():
    global tiempo_inicio

    address = ADDRESS
    if address == "XX:XX:XX:XX:XX:XX":
        address = await escanear_arduino()
        if address is None:
            print("✗ No se encontró ningún dispositivo PingPong_Data.")
            print("  Comprueba que el Arduino está encendido y el LED es azul.")
            return

    print(f"Conectando a {address}...")
    async with BleakClient(address) as client:
        if not client.is_connected:
            print("✗ No se pudo conectar.")
            return

        print(f"✓ Conectado. Grabando {TIEMPO_CAPTURA_SEG}s → {ARCHIVO_SALIDA}")
        print(f"  Realiza los golpes ahora.\n")

        tiempo_inicio = time.time()
        await client.start_notify(UUID_IMU, recepcion_datos)

        # Mostrar progreso durante la captura
        while (time.time() - tiempo_inicio) < TIEMPO_CAPTURA_SEG:
            await asyncio.sleep(1.0)
            elapsed = time.time() - tiempo_inicio
            freq_est = paquetes_ok / elapsed if elapsed > 0 else 0
            print(f"  t={elapsed:.0f}s — {paquetes_ok} muestras — {freq_est:.1f} Hz", end='\r')

        await client.stop_notify(UUID_IMU)

    tiempo_total = time.time() - tiempo_inicio
    freq_real    = paquetes_ok / tiempo_total if tiempo_total > 0 else 0

    print(f"\n✓ Captura finalizada.")
    print(f"  Muestras OK:  {paquetes_ok}")
    print(f"  Paquetes err: {paquetes_err}")
    print(f"  Duración:     {tiempo_total:.1f}s")
    print(f"  Freq. real:   {freq_real:.1f} Hz (objetivo: 100 Hz)")

    if freq_real < 85:
        print(f"  ⚠ Frecuencia baja — posible pérdida de paquetes BLE.")
        print(f"    Reducir distancia al PC o interferencias WiFi/BT cercanas.")

    # Guardar CSV con coma (compatible Edge Impulse)
    with open(ARCHIVO_SALIDA, mode='w', newline='') as f:
        writer = csv.writer(f, delimiter=',')
        writer.writerow(["timestamp", "accX", "accY", "accZ", "gyrX", "gyrY", "gyrZ"])
        writer.writerows(datos)

    print(f"✓ Guardado: {os.path.abspath(ARCHIVO_SALIDA)}")
    print(f"  Sube a Edge Impulse: Data Acquisition → Upload existing data")

if __name__ == "__main__":
    asyncio.run(run())
