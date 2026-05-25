/* =============================================================
 *  Clasificador de Golpes de Ping Pong
 *  Arduino Nano 33 BLE Sense + Edge Impulse
 * 
 *  Estrategia de ventana:
 *    - Monitoriza magnitud de aceleracion continuamente
 *    - Cuando detecta pico > 150 m/s2 marca el momento
 *    - Captura 200ms ANTES del pico + 800ms DESPUES
 *    - Clasifica esa ventana de 1000ms centrada en el golpe
 *    - Cooldown de 1200ms entre detecciones
 * 
 *  Golpes:
 *    - Saque   → LED Azul
 *    - Derecha → LED Verde
 *    - Reves   → LED Amarillo
 *    - Cortado → LED Rojo
 *    - Mate    → LED Blanco
 *    - No golpe (confianza < 0.92) → LED Apagado
 * =============================================================*/

#include <PING-PONG_inferencing.h>
#include <Arduino_LSM9DS1.h>
#include <Arduino_LPS22HB.h>
#include <Arduino_HTS221.h>
#include <Arduino_APDS9960.h>

//Bloetoth-----------
#include <ArduinoBLE.h> // LIBRERÍA BLE

// UUIDs inventados para tu servicio de Ping Pong
BLEService pingPongService("19B10000-E8F2-537E-4F6C-D104768A1214"); 

// Característica de tipo String que permitirá Leer (BLERead) y recibir Notificaciones (BLENotify)
// Le damos un tamaño máximo de 20 bytes (suficiente para "derecha (95.0%)")
BLEStringCharacteristic golpeChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 30);

// NUEVA Característica para los descartes (fíjate que el UUID acaba en 15)
BLEStringCharacteristic descarteChar("19B10002-E8F2-537E-4F6C-D104768A1215", BLERead | BLENotify, 30);

// NUEVA Característica para encender/apagar (UUID termina en 16)
BLEByteCharacteristic controlChar("19B10003-E8F2-537E-4F6C-D104768A1216", BLERead | BLEWrite);

// Variable para saber si el sistema debe clasificar o no
bool sistema_activo = true;
//------------------

/* Constantes -------------------------------------------------------------- */
#define CONVERT_G_TO_MS2        9.80665f
#define MAX_ACCEPTED_RANGE      2.0f
#define N_SENSORS               18

// Umbral de confianza minima para aceptar una clasificacion
#define UMBRAL_CONFIANZA        0.92f

// Umbral de magnitud de aceleracion para detectar un golpe
// Reposo = 50-80 m/s2, golpes = 200-1493 m/s2
#define UMBRAL_PICO             14.0f

// Muestras antes del pico a incluir en la ventana (200ms a 100Hz = 20 muestras)
#define MUESTRAS_PRE_PICO       20

// Muestras despues del pico a esperar antes de clasificar
// 800ms a 100Hz = 80 muestras. Total ventana = 20+80 = 100 muestras = 1000ms
#define MUESTRAS_POST_PICO      60

// Cooldown minimo entre dos golpes detectados (ms)
#define COOLDOWN_MS             1200

/* Pines LED RGB (catodo comun: LOW = encendido) --------------------------- */
#define PIN_LED_R   22
#define PIN_LED_G   23
#define PIN_LED_B   24

/* Estados de la maquina de estados ---------------------------------------- */
enum Estado {
    ESPERANDO,      // Monitorizando, esperando pico
    POST_PICO,      // Pico detectado, recogiendo muestras post-pico
    CLASIFICANDO    // Ventana completa, clasificar
};

/* Tipos ------------------------------------------------------------------- */
enum sensor_status { NOT_USED = -1, NOT_INIT, INIT, SAMPLED };

typedef struct {
    const char   *name;
    float        *value;
    uint8_t     (*poll_sensor)(void);
    bool        (*init_sensor)(void);
    sensor_status status;
} eiSensors;

/* Variables --------------------------------------------------------------- */
static const bool debug_nn = false;
static float data[N_SENSORS];
static int8_t fusion_sensors[N_SENSORS];
static int    fusion_ix = 0;

// Buffer circular — guarda siempre los ultimos 1000ms (100 muestras x N_ejes)
// Mas un margen extra por si acaso
static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };
static float ventana_golpe[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

// Maquina de estados
static Estado estado = ESPERANDO;
static uint32_t muestras_post = 0;       // Contador muestras tras el pico
static uint32_t muestras_acumuladas = 0; // Para saber si el buffer esta lleno

// Control cooldown
static unsigned long ultimo_golpe_ms = 0;

/* Declaraciones adelantadas ----------------------------------------------- */
float    ei_get_sign(float number);
bool     init_IMU(void); bool init_HTS(void);
bool     init_BARO(void); bool init_APDS(void);
uint8_t  poll_acc(void); uint8_t poll_gyr(void); uint8_t poll_mag(void);
uint8_t  poll_HTS(void); uint8_t poll_BARO(void);
uint8_t  poll_APDS_color(void); uint8_t poll_APDS_proximity(void);
uint8_t  poll_APDS_gesture(void);
void     set_led(int r, int g, int b);
void     led_segun_golpe(const char *golpe);
void     clasificar_ventana(void);
static bool   ei_connect_fusion_list(const char *input_list);
static int8_t ei_find_axis(char *axis_name);

/* Tabla de sensores ------------------------------------------------------- */
eiSensors sensors[] = {
    {"accX",       &data[0],  &poll_acc,            &init_IMU,  NOT_USED},
    {"accY",       &data[1],  &poll_acc,            &init_IMU,  NOT_USED},
    {"accZ",       &data[2],  &poll_acc,            &init_IMU,  NOT_USED},
    {"gyrX",       &data[3],  &poll_gyr,            &init_IMU,  NOT_USED},
    {"gyrY",       &data[4],  &poll_gyr,            &init_IMU,  NOT_USED},
    {"gyrZ",       &data[5],  &poll_gyr,            &init_IMU,  NOT_USED},
    {"magX",       &data[6],  &poll_mag,            &init_IMU,  NOT_USED},
    {"magY",       &data[7],  &poll_mag,            &init_IMU,  NOT_USED},
    {"magZ",       &data[8],  &poll_mag,            &init_IMU,  NOT_USED},
    {"temperature",&data[9],  &poll_HTS,            &init_HTS,  NOT_USED},
    {"humidity",   &data[10], &poll_HTS,            &init_HTS,  NOT_USED},
    {"pressure",   &data[11], &poll_BARO,           &init_BARO, NOT_USED},
    {"red",        &data[12], &poll_APDS_color,     &init_APDS, NOT_USED},
    {"green",      &data[13], &poll_APDS_color,     &init_APDS, NOT_USED},
    {"blue",       &data[14], &poll_APDS_color,     &init_APDS, NOT_USED},
    {"brightness", &data[15], &poll_APDS_color,     &init_APDS, NOT_USED},
    {"proximity",  &data[16], &poll_APDS_proximity, &init_APDS, NOT_USED},
    {"gesture",    &data[17], &poll_APDS_gesture,   &init_APDS, NOT_USED},
};

/* ==========================================================================
 *  SETUP
 * ========================================================================== */
void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    set_led(0, 0, 0);

    Serial.println("=== Clasificador Ping Pong ===");
    Serial.print("Umbral pico deteccion: "); Serial.print(UMBRAL_PICO); Serial.println(" m/s2");
    Serial.print("Umbral confianza:      "); Serial.println(UMBRAL_CONFIANZA);
    Serial.print("Pre-pico:  "); Serial.print(MUESTRAS_PRE_PICO*10); Serial.println("ms");
    Serial.print("Post-pico: "); Serial.print(MUESTRAS_POST_PICO*10); Serial.println("ms");
    Serial.println("Esperando golpes...\n");

    if (!ei_connect_fusion_list(EI_CLASSIFIER_FUSION_AXES_STRING)) {
        ei_printf("ERR: Error en lista de sensores\r\n");
        return;
    }

    for (int i = 0; i < fusion_ix; i++) {
        if (sensors[fusion_sensors[i]].status == NOT_INIT) {
            sensors[fusion_sensors[i]].status =
                (sensor_status)sensors[fusion_sensors[i]].init_sensor();
        }
    }

  // --- INICIO CONFIGURACIÓN BLE ---
  if (!BLE.begin()) {
      Serial.println("ERR: ¡Falló la inicialización del módulo BLE!");
      while (1);
  }

  // Configuramos el nombre que verás en el móvil
  BLE.setLocalName("NanoPingPong");
  BLE.setAdvertisedService(pingPongService);
  
  // Añadimos la característica al servicio y el servicio al BLE
  pingPongService.addCharacteristic(golpeChar);
  pingPongService.addCharacteristic(descarteChar); // <-- AÑADIDO
  pingPongService.addCharacteristic(controlChar);  // <-- NUEVO INTERRUPTOR
  BLE.addService(pingPongService);
  
  // Valor inicial
  golpeChar.writeValue("Esperando...");
  descarteChar.writeValue("Sin descartes..."); // <-- AÑADIDO
  controlChar.writeValue((byte)1);                 // <-- EMPIEZA ENCENDIDO (1)
  
  // Empezamos a emitir
  BLE.advertise();
  Serial.println("BLE Activo. Emitiendo advertising como 'NanoPingPong'...");
  // --- FIN CONFIGURACIÓN BLE ---

}

/* ==========================================================================
 *  LOOP — Maquina de estados para captura centrada en el golpe
 * ========================================================================== */
void loop() {
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != fusion_ix) {
        ei_printf("ERR: Sensores no coinciden con el modelo\r\n");
        delay(5000);
        return;
    }

    // Tick para mantener 100Hz exacto
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000UL);

    // --- INICIO CÓDIGO BLE NUEVO ---
    BLE.poll(); // Atender conexiones Bluetooth
    
    if (controlChar.written()) {
        if (controlChar.value() == 0) {
            sistema_activo = false;
            estado = ESPERANDO; // Reiniciamos por si estaba a medias de un golpe
            Serial.println(">> Sistema PAUSADO desde el movil");
        } else {
            sistema_activo = true;
            Serial.println(">> Sistema REANUDADO desde el movil");
        }
    }
    // --- FIN CÓDIGO BLE NUEVO ---

    // --- Leer sensores y meter en buffer circular ---
    numpy::roll(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -fusion_ix);

    for (int i = 0; i < fusion_ix; i++) {
        if (sensors[fusion_sensors[i]].status == INIT) {
            sensors[fusion_sensors[i]].poll_sensor();
            sensors[fusion_sensors[i]].status = SAMPLED;
        }
        if (sensors[fusion_sensors[i]].status == SAMPLED) {
            buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - fusion_ix + i] =
                *sensors[fusion_sensors[i]].value;
            sensors[fusion_sensors[i]].status = INIT;
        }
    }

    if (muestras_acumuladas < (uint32_t)EI_CLASSIFIER_RAW_SAMPLE_COUNT)
        muestras_acumuladas++;

    // --- Calcular magnitud de aceleracion de la muestra actual ---
    float ax = buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - fusion_ix + 0];
    float ay = buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - fusion_ix + 1];
    float az = buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - fusion_ix + 2];
    float magnitud = sqrt(ax*ax + ay*ay + az*az);

    // DEBUG TEMPORAL - quitar cuando funcione
   // Serial.print("mag=");
   // Serial.println(magnitud, 2);
    //----------------

    // --- Maquina de estados ---
    unsigned long ahora = millis();
    bool buffer_lleno   = (muestras_acumuladas >= (uint32_t)EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    bool cooldown_ok    = (ahora - ultimo_golpe_ms) > COOLDOWN_MS;
    if (sistema_activo) {
      switch (estado) {

          case ESPERANDO:
              // Buscar pico que indique inicio de golpe
              if (buffer_lleno && cooldown_ok && magnitud > UMBRAL_PICO) {
                  // Pico detectado — empezar a contar muestras post-pico
                  estado = POST_PICO;
                  muestras_post = 0;
                  // El buffer ya contiene los 200ms anteriores al pico
                  // porque es circular y siempre tiene los ultimos 1000ms
              }
              break;

          case POST_PICO:
              // Seguir acumulando hasta completar las muestras post-pico
              muestras_post++;
              if (muestras_post >= MUESTRAS_POST_PICO) {
                  estado = CLASIFICANDO;
              }
              break;

          case CLASIFICANDO:
              // Copiar el buffer actual a la ventana de clasificacion
              // En este momento el buffer contiene:
              // [200ms pre-pico][pico][800ms post-pico] = 1000ms exactos
              memcpy(ventana_golpe, buffer,
                    EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(float));
              clasificar_ventana();
              ultimo_golpe_ms = millis();
              estado = ESPERANDO;
              break;
      }
    } // <-- ESTA LLAVE CIERRA EL IF (SISTEMA_ACTIVO)
    // Esperar al proximo tick
    uint64_t time_to_wait = next_tick - micros();
    if (time_to_wait > 0 && time_to_wait < 100000UL) {
        delay((int)floor((float)time_to_wait / 1000.0f));
        delayMicroseconds(time_to_wait % 1000);
    }
}

/* ==========================================================================
 *  CLASIFICAR VENTANA
 * ========================================================================== */
void clasificar_ventana() {
    signal_t signal;
    int err = numpy::signal_from_buffer(
        ventana_golpe, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    if (err != 0) {
        ei_printf("ERR: signal_from_buffer (%d)\n", err);
        return;
    }

    ei_impulse_result_t result = { 0 };
    err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: run_classifier (%d)\n", err);
        return;
    }

    // Buscar clase con mayor confianza
    float max_conf    = 0.0f;
    const char *mejor = "no_golpe";

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > max_conf) {
            max_conf = result.classification[ix].value;
            mejor    = result.classification[ix].label;
        }
    }

    if (max_conf >= UMBRAL_CONFIANZA) {
        Serial.print(">> GOLPE: ");
        Serial.print(mejor);
        Serial.print("  (");
        Serial.print(max_conf * 100, 1);
        Serial.println("%)");


        // --- INICIO CÓDIGO BLE NUEVO ---
        String mensajeBLE =  String(mejor) + " (" + String(max_conf * 100, 1) + "%)";
        golpeChar.writeValue(mensajeBLE); 
        // --- FIN CÓDIGO BLE NUEVO ---


        led_segun_golpe(mejor);
        delay(800);
        set_led(0, 0, 0);
    } else {
        // Confianza insuficiente — imprimir para debug
        Serial.print("   [descartado: ");
        Serial.print(mejor);
        Serial.print(" ");
        Serial.print(max_conf * 100, 1);
        Serial.println("%]");

        // --- ENVIAR DESCARTE POR BLE ---
        String mensajeMalo = " X " + String(mejor) + " (" + String(max_conf * 100, 1) + "%)";
        descarteChar.writeValue(mensajeMalo);
        // -------------------------------
    }
}

/* ==========================================================================
 *  LED RGB
 * ========================================================================== */
void set_led(int r, int g, int b) {
    digitalWrite(PIN_LED_R, r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

String obtener_emoji(const char *golpe) {
    if      (strcmp(golpe, "saque")   == 0) return "🟦 "; // Azul
    else if (strcmp(golpe, "derecha") == 0) return "🟩 "; // Verde
    else if (strcmp(golpe, "reves")   == 0) return "🟨 "; // Amarillo
    else if (strcmp(golpe, "cortado") == 0) return "🟥 "; // Rojo
    else if (strcmp(golpe, "mate")    == 0) return "⬜ "; // Blanco
    return "";
}

void led_segun_golpe(const char *golpe) {
    if      (strcmp(golpe, "saque")   == 0) set_led(0, 0, 1); // Azul
    else if (strcmp(golpe, "derecha") == 0) set_led(0, 1, 0); // Verde
    else if (strcmp(golpe, "reves")   == 0) set_led(1, 1, 0); // Amarillo
    else if (strcmp(golpe, "cortado") == 0) set_led(1, 0, 0); // Rojo
    else if (strcmp(golpe, "mate")    == 0) set_led(1, 1, 1); // Blanco
    else                                    set_led(0, 0, 0); // Apagado
}

/* ==========================================================================
 *  FUNCIONES DE SENSOR
 * ========================================================================== */
float ei_get_sign(float number) { return (number >= 0.0) ? 1.0 : -1.0; }

bool init_IMU(void)  { static bool s=false; if(!s) s=IMU.begin();  return s; }
bool init_HTS(void)  { static bool s=false; if(!s) s=HTS.begin();  return s; }
bool init_BARO(void) { static bool s=false; if(!s) s=BARO.begin(); return s; }
bool init_APDS(void) { static bool s=false; if(!s) s=APDS.begin(); return s; }

uint8_t poll_acc(void) {
    if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(data[0], data[1], data[2]);
        for (int i = 0; i < 3; i++) {
            if (fabs(data[i]) > MAX_ACCEPTED_RANGE)
                data[i] = ei_get_sign(data[i]) * MAX_ACCEPTED_RANGE;
        }
        data[0] *= CONVERT_G_TO_MS2;
        data[1] *= CONVERT_G_TO_MS2;
        data[2] *= CONVERT_G_TO_MS2;
    }
    return 0;
}

uint8_t poll_gyr(void) {
    if (IMU.gyroscopeAvailable())
        IMU.readGyroscope(data[3], data[4], data[5]);
    return 0;
}

uint8_t poll_mag(void) {
    if (IMU.magneticFieldAvailable())
        IMU.readMagneticField(data[6], data[7], data[8]);
    return 0;
}

uint8_t poll_HTS(void) {
    data[9]  = HTS.readTemperature();
    data[10] = HTS.readHumidity();
    return 0;
}

uint8_t poll_BARO(void) { data[11] = BARO.readPressure(); return 0; }

uint8_t poll_APDS_color(void) {
    int t[4];
    if (APDS.colorAvailable()) {
        APDS.readColor(t[0], t[1], t[2], t[3]);
        data[12]=t[0]; data[13]=t[1]; data[14]=t[2]; data[15]=t[3];
    }
    return 0;
}

uint8_t poll_APDS_proximity(void) {
    if (APDS.proximityAvailable()) data[16] = (float)APDS.readProximity();
    return 0;
}

uint8_t poll_APDS_gesture(void) {
    if (APDS.gestureAvailable()) data[17] = (float)APDS.readGesture();
    return 0;
}

/* ==========================================================================
 *  UTILIDADES FUSION
 * ========================================================================== */
static int8_t ei_find_axis(char *axis_name) {
    for (int ix = 0; ix < N_SENSORS; ix++)
        if (strstr(axis_name, sensors[ix].name)) return ix;
    return -1;
}

static bool ei_connect_fusion_list(const char *input_list) {
    char *input_string = (char *)ei_malloc(strlen(input_list) + 1);
    if (!input_string) return false;
    memset(input_string, 0, strlen(input_list) + 1);
    strncpy(input_string, input_list, strlen(input_list));
    memset(fusion_sensors, 0, N_SENSORS);
    fusion_ix = 0;
    bool is_fusion = false;
    char *buff = strtok(input_string, "+");
    while (buff != NULL) {
        int8_t found = ei_find_axis(buff);
        if (found >= 0 && fusion_ix < N_SENSORS) {
            fusion_sensors[fusion_ix++] = found;
            sensors[found].status = NOT_INIT;
            is_fusion = true;
        }
        buff = strtok(NULL, "+ ");
    }
    ei_free(input_string);
    return is_fusion;
}

#if !defined(EI_CLASSIFIER_SENSOR) || \
    (EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_FUSION && \
     EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_ACCELEROMETER)
#error "Modelo no valido para este sensor"
#endif
