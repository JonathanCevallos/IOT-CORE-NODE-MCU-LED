#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include "secrets.h"

#define ZONA_HORARIA -5
#define PIN_LED_AZUL D1
#define PIN_LED_ROJO D3
#define PIN_LED_VERDE D0

float temperatura;
float humedad;

unsigned long ultimoMillis = 0;
const long intervalo = 5000;

#define TOPICO_PUBLICACION_AWS "esp8266/pub"
#define TOPICO_SUBSCRIPCION_AWS "esp8266/led"

WiFiClientSecure clienteRed;
BearSSL::X509List certificado(cacert);
BearSSL::X509List certificado_cliente(client_cert);
BearSSL::PrivateKey llave(privkey);

PubSubClient cliente(clienteRed);

time_t ahora;
time_t momento_actual = 1510592825;

void conectarNTP(void)
{
  Serial.print("Configurando la hora utilizando SNTP");
  configTime(ZONA_HORARIA * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  ahora = time(nullptr);
  while (ahora < momento_actual)
  {
    delay(500);
    Serial.print(".");
    ahora = time(nullptr);
  }
  Serial.println("¡hecho!");
  struct tm informacion_tiempo;
  gmtime_r(&ahora, &informacion_tiempo);
  Serial.print("Hora actual: ");
  Serial.print(asctime(&informacion_tiempo));
}

void mensajeRecibido(char *topico, byte *carga, unsigned int longitud)
{
  Serial.print("Recibido [");
  Serial.print(topico);
  Serial.print("]: ");

  String mensaje = "";
  for (int i = 0; i < longitud; i++)
  {
    mensaje += (char)carga[i];
  }

  Serial.println(mensaje);

  if (mensaje == "BLUE")
  {
    digitalWrite(PIN_LED_AZUL, HIGH);
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, LOW);
    Serial.println("LED Azul Encendido");
  }
  else if (mensaje == "RED")
  {
    digitalWrite(PIN_LED_AZUL, LOW);
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
    Serial.println("LED Rojo Encendido");
  }
  else if (mensaje == "GREEN")
  {
    digitalWrite(PIN_LED_AZUL, LOW);
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
    Serial.println("LED Verde Encendido");
  }
  else if (mensaje == "OFF")
  {
    digitalWrite(PIN_LED_AZUL, LOW);
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, LOW);
    Serial.println("Todos los LEDs Apagados");
  }
}

void conectarAWS()
{
  WiFiManager gestorWiFi;
  gestorWiFi.autoConnect("NODE MCU");

  Serial.println("¡Conectado a WiFi!");

  conectarNTP();

  clienteRed.setTrustAnchors(&certificado);
  clienteRed.setClientRSACert(&certificado_cliente, &llave);

  cliente.setServer(MQTT_HOST, 8883);
  cliente.setCallback(mensajeRecibido);

  Serial.println("Conectando a AWS IoT");

  while (!cliente.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }

  if (!cliente.connected())
  {
    Serial.println("¡Tiempo de espera de AWS IoT!");
    return;
  }

  cliente.subscribe(TOPICO_SUBSCRIPCION_AWS);

  Serial.println("¡Conectado a AWS IoT!");
}

void publicarMensaje()
{
  StaticJsonDocument<200> documento;
  documento["Tiempo"] = millis();
  documento["Humedad"] = humedad;
  documento["Temperatura"] = temperatura;
  char bufferJson[512];
  serializeJson(documento, bufferJson);
  cliente.publish(TOPICO_PUBLICACION_AWS, bufferJson);
}

void setup()
{
  pinMode(PIN_LED_AZUL, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  Serial.begin(115200);

  conectarAWS();
}

void loop()
{
  humedad = 12.5;
  temperatura = 20.54;

  if (isnan(humedad) || isnan(temperatura))
  {
    Serial.println(F("¡Error al leer desde el sensor DHT!"));
    return;
  }

  Serial.print(F("Humedad: "));
  Serial.print(humedad);
  Serial.print(F("%  Temperatura: "));
  Serial.print(temperatura);
  Serial.println(F("°C "));
  delay(2000);

  ahora = time(nullptr);

  if (!cliente.connected())
  {
    conectarAWS();
  }
  else
  {
    cliente.loop();
    if (millis() - ultimoMillis > 5000)
    {
      ultimoMillis = millis();
      publicarMensaje();
    
    }
  }
}
