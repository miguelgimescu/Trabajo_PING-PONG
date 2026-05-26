/*
 * extraccion_ble.ino
 * Arduino Nano 33 BLE Sense — Extracción inalámbrica de datos IMU a 100Hz
 * 
 * El Arduino va montado en la pala alimentado por batería externa.
 * Envía los datos del acelerómetro y giroscopio por Bluetooth Low Energy
 * para ser recibidos por el script Python en el PC.
 * 
 * Formato de envío BLE (string optimizado, < 35 bytes):
 * "ax,ay,az,gx,gy,gz"
 * Ejemplo: "0.12,-0.34,1.01,2.3,-1.2,0.5"
 */

#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>

/* Servicio y característica BLE ------------------------------------------- */
BLEService imuService("19B10010-E8F2-537E-4F6C-D104768A1214");
BLECharacteristic imuChar("19B10011-E8F2-537E-4F6C-D104768A1214",
                           BLERead | BLENotify, 40);

/* Constantes -------------------------------------------------------------- */
const long INTERVALO_MS = 10; // 10ms = 100Hz exactos

/* Pines LED (feedback visual sin cable) ----------------------------------- */
#define PIN_LED_R 22
#define PIN_LED_G 23
#define PIN_LED_B 24

unsigned long tiempoAnterior = 0;

void setup() {
    Serial.begin(115200);
    // NO bloqueamos en while(!Serial) para funcionar sin PC conectado

    // LED init
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    digitalWrite(PIN_LED_R, HIGH); // Apagado (catodo comun)
    digitalWrite(PIN_LED_G, HIGH);
    digitalWrite(PIN_LED_B, HIGH);

    // Inicializar IMU
    if (!IMU.begin()) {
        // LED rojo parpadeante = error IMU
        while (1) {
            digitalWrite(PIN_LED_R, LOW); delay(200);
            digitalWrite(PIN_LED_R, HIGH); delay(200);
        }
    }

    // Inicializar BLE
    if (!BLE.begin()) {
        // LED azul parpadeante = error BLE
        while (1) {
            digitalWrite(PIN_LED_B, LOW); delay(200);
            digitalWrite(PIN_LED_B, HIGH); delay(200);
        }
    }

    BLE.setLocalName("PingPong_Data");
    BLE.setAdvertisedService(imuService);
    imuService.addCharacteristic(imuChar);
    BLE.addService(imuService);
    BLE.advertise();

    // LED azul fijo = esperando conexion
    digitalWrite(PIN_LED_B, LOW);
}

void loop() {
    BLEDevice central = BLE.central();

    if (central) {
        // LED verde = conectado, grabando
        digitalWrite(PIN_LED_B, HIGH);
        digitalWrite(PIN_LED_G, LOW);

        while (central.connected()) {
            unsigned long tiempoActual = millis();

            if (tiempoActual - tiempoAnterior >= INTERVALO_MS) {
                tiempoAnterior = tiempoActual;

                float ax, ay, az, gx, gy, gz;

                if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
                    IMU.readAcceleration(ax, ay, az);
                    IMU.readGyroscope(gx, gy, gz);

                    // Empaquetamos en string minimizando decimales
                    // para reducir el payload y evitar perdida de paquetes
                    String datos = String(ax, 2) + "," +
                                   String(ay, 2) + "," +
                                   String(az, 2) + "," +
                                   String(gx, 1) + "," +
                                   String(gy, 1) + "," +
                                   String(gz, 1);

                    imuChar.writeValue((const uint8_t*)datos.c_str(),
                                       datos.length());
                }
            }
        }

        // Desconectado: volver a LED azul esperando
        digitalWrite(PIN_LED_G, HIGH);
        digitalWrite(PIN_LED_B, LOW);
        BLE.advertise();
    }
}
