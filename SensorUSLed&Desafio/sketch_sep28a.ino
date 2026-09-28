// Laboratorio 5 - Ultrasónico + LED RGB + Adafruit IO (incluye desafío del Toggle)
//
// Conexiones:
//   HC-SR04 VCC -> 5V | GND -> GND | TRIG -> GPIO18 | ECHO -> GPIO19
//   LED RGB: R -> 330Ω -> GPIO25 | G -> 330Ω -> GPIO26 | B -> 330Ω -> GPIO27 | Cátodo -> GND
//
// Feeds en Adafruit IO:
//   distancia  -> lo publica el ESP32 (Gauge)
//   led        -> lo publica el dashboard (Toggle) y el ESP32 se suscribe

#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ---------------------- CONFIGURACIÓN WI-FI ----------------------
#define WLAN_SSID "ARTEFACTOS"
#define WLAN_PASS "87654321"

// ---------------------- CONFIGURACIÓN ADAFRUIT IO ----------------------
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "GabrielEAG"
#define AIO_KEY         "clave"   

// ---------------------- PINES ----------------------
#define TRIG_PIN 18
#define ECHO_PIN 19
#define PIN_R 25
#define PIN_G 26
#define PIN_B 27

// ---------------------- AJUSTES ----------------------
#define RGB_ANODO_COMUN false
#define PWM_FREQ 5000
#define PWM_RES 8
#define INTERVALO_PUBLICAR_MS 5000   // Adafruit gratis limita los datos por minuto: no bajar de 4-5 s
#define DIST_CERCA_CM 15             // menor a esto -> ROJO
#define DIST_MEDIA_CM 40             // menor a esto -> AMARILLO, si no -> VERDE

// ---------------------- CLIENTE MQTT Y FEED ----------------------
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Publicar
Adafruit_MQTT_Publish feedDistancia = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/distancia");

// Suscribirse (Toggle del dashboard)
Adafruit_MQTT_Subscribe feedLED = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led");

// ---------------------- ESTADO ----------------------
float ultimaDistancia = -1;   // última lectura válida (cm)
bool ledEncendido = true;     // estado del Toggle (por defecto encendido)
unsigned long ultimoPublicar = 0;
unsigned long ultimoPing = 0;

// ---------------------- PROTOTIPOS ----------------------
void conectarWiFi();
void conectarMQTT();
float leerDistanciaCm();
float distanciaPromedio(int muestras);
void escribirRGB(uint8_t r, uint8_t g, uint8_t b);
void actualizarLED();

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  ledcAttach(PIN_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_B, PWM_FREQ, PWM_RES);

  escribirRGB(255, 255, 255);

  conectarWiFi();

  mqtt.subscribe(&feedLED);   // suscripción al feed del Toggle
}

// =====================================================================
void loop() {
  conectarMQTT();   // mantiene la conexión con Adafruit (reconecta si se cae)

  // Revisar si llegó un mensaje nuevo del Toggle
  Adafruit_MQTT_Subscribe *suscripcion;
  while ((suscripcion = mqtt.readSubscription(100))) {
    if (suscripcion == &feedLED) {
      String mensaje = (char *)feedLED.lastread;
      Serial.print("Toggle LED: "); Serial.println(mensaje);
      ledEncendido = (mensaje == "ON" || mensaje == "1");
      actualizarLED();
    }
  }

  // Leer el ultrasónico y publicar cada cierto tiempo
  if (millis() - ultimoPublicar >= INTERVALO_PUBLICAR_MS) {
    ultimoPublicar = millis();

    float d = distanciaPromedio(5);

    if (d > 0) {
      ultimaDistancia = d;
      Serial.print("Distancia: "); Serial.print(d, 1); Serial.println(" cm");
      if (!feedDistancia.publish(d)) {
        Serial.println("Error al publicar la distancia");
      }
    } else {
      Serial.println("Lectura fuera de rango o sin eco");
    }

    actualizarLED();   // el color depende de la última distancia leída
  }

  // Mantener viva la conexión MQTT
  if (millis() - ultimoPing >= 30000) {
    ultimoPing = millis();
    mqtt.ping();
  }
}

// =====================================================================
// ULTRASÓNICO HC-SR04
// =====================================================================
float leerDistanciaCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return -1;   // no hubo eco

  float distancia = (0.0343 * duracion) / 2;

  // Rango útil del sensor: 2 a 100 cm
  if (distancia < 2 || distancia > 100) return -1;
  return distancia;
}

// Promedia varias lecturas para reducir el ruido
float distanciaPromedio(int muestras) {
  float suma = 0;
  int validas = 0;
  for (int i = 0; i < muestras; i++) {
    float d = leerDistanciaCm();
    if (d > 0) { suma += d; validas++; }
    delay(40);   // espera entre mediciones para que no se mezclen los ecos
  }
  return (validas > 0) ? suma / validas : -1;
}

// =====================================================================
// LED RGB
// =====================================================================
void escribirRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (RGB_ANODO_COMUN) {
    r = 255 - r; g = 255 - g; b = 255 - b;
  }
  ledcWrite(PIN_R, r);
  ledcWrite(PIN_G, g);
  ledcWrite(PIN_B, b);
}

// El color debe cambiar solo según la última distancia leída.
void actualizarLED() {
  // Toggle en OFF -> LED apagado
  if (!ledEncendido) {
    escribirRGB(0, 0, 0);
    return;
  }

  if (ultimaDistancia <= 0) {
    escribirRGB(255, 255, 255);   // sin lectura válida -> blanco
    return;
  }

  if (ultimaDistancia < DIST_CERCA_CM) {
    escribirRGB(255, 0, 0);       // cerca -> rojo
  } else if (ultimaDistancia < DIST_MEDIA_CM) {
    escribirRGB(255, 255, 0);     // medio -> amarillo
  } else {
    escribirRGB(0, 255, 0);       // lejos -> verde
  }
}

// =====================================================================
// CONEXIONES
// =====================================================================
void conectarWiFi() {
  Serial.print("Conectando a "); Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi conectado. IP: "); Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Conectando a Adafruit IO... ");
  int8_t ret;
  uint8_t intentos = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Reintentando en 5 segundos...");
    mqtt.disconnect();
    delay(5000);
    if (--intentos == 0) {
      Serial.println("No se pudo conectar. Reiniciando la ESP32...");
      ESP.restart();
    }
  }
  Serial.println("¡Conectado a Adafruit IO!");
}
