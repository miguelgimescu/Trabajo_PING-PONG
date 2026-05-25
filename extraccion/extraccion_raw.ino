/*
 * extraccion_raw.ino
 * Arduino Nano 33 BLE Sense — Extracción de datos IMU a 100Hz
 * 
 * Envía por puerto serie (115200 baud) los datos del acelerómetro
 * y giroscopio en formato CSV para su captura con Python.
 * 
 * Formato de salida:
 * timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
 */

#include <Arduino_LSM9DS1.h>

unsigned long tiempoAnterior = 0;
const long intervalo = 10; // 10ms = 100Hz exactos

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!IMU.begin()) {
    Serial.println("Error al inicializar el IMU!");
    while (1);
  }

  // Cabecera CSV
  Serial.println("timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ");
}

void loop() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - tiempoAnterior >= intervalo) {
    tiempoAnterior = tiempoActual;

    float aX, aY, aZ;
    float gX, gY, gZ;

    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);

      Serial.print(tiempoActual); Serial.print(",");
      Serial.print(aX, 4);       Serial.print(",");
      Serial.print(aY, 4);       Serial.print(",");
      Serial.print(aZ, 4);       Serial.print(",");
      Serial.print(gX, 4);       Serial.print(",");
      Serial.print(gY, 4);       Serial.print(",");
      Serial.println(gZ, 4);
    }
  }
}
