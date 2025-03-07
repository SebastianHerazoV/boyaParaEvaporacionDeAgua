#include <WiFi.h>
#include <ESPmDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>     

// ----- Pin Definitions -----
#define ONE_WIRE_BUS 5      
#define OUTPUT_PIN 4      
#define DHT_PIN 13        

// ----- Credenciales de acceso -----
const char* ap_ssid = "A58SEBAS";        
const char* ap_password = "1101876738";    

// ----- Creacion de objetos -----
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DHT dht(DHT_PIN, DHT11); 

// ----- Variables y limites -----
DeviceAddress sensorSurface;  
DeviceAddress sensorDeep;     
const float TEMP_DIFF_THRESHOLD = 2.0; 
const float HUMIDITY_THRESHOLD = 70.0;  

WiFiServer server(80); 

void setup() {
  // Iniciar comunicación serial
  Serial.begin(115200);

  // Inicialización sensores temp
  tempSensors.begin();

  // guardar el address de los sensores
  if (!tempSensors.getAddress(sensorSurface, 0)) {
    Serial.println("Unable to find address for DS18B20 sensor 0 (surface).");
    while (1); // esperar si no es encontrado
  }
  if (!tempSensors.getAddress(sensorDeep, 1)) {
    Serial.println("Unable to find address for DS18B20 sensor 1 (deep).");
    while (1); // esperar si no es encontrado
  }

  // Inicialización sensor humedad
  dht.begin();
  Serial.println("DHT11 sensor initialized.");

  // definicion pin actuador
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW); // Ensure the output pin is initially off

  // iniciar esp32 como Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point started. Connect to Wi-Fi SSID: ");
  Serial.println(ap_ssid);

  // imprimir ip
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP Address: ");
  Serial.println(IP);

  // iniciar mDNS
  if (!MDNS.begin("esp")) { // Set the hostname to "esp"
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started. Access the ESP32 at http://esp.local");
  }

  server.begin(); // iniciar servidor web
}

void loop() {
  // Esperar conexion de usuario
  WiFiClient client = server.available();
  if (client) {
    String currentLine = ""; // guardar HTTP request
    bool requestComplete = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') { 
          if (currentLine.length() == 0) {
            requestComplete = true;
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    if (requestComplete) {
      // Pedir mediciones a los sensores DS18B20
      tempSensors.requestTemperatures();
      float tempSurface = tempSensors.getTempC(sensorSurface);
      float tempDeep = tempSensors.getTempC(sensorDeep);

      // pedir mediciones a sensor de humedad dht11
      float dhtTemp = dht.readTemperature(); // Temperature in Celsius
      float dhtHumidity = dht.readHumidity(); // Humidity in %

      // Comprobar sensores desconectados
      if (tempSurface == DEVICE_DISCONNECTED_C || tempDeep == DEVICE_DISCONNECTED_C ||
          isnan(dhtTemp) || isnan(dhtHumidity)) {
        client.println(F("<html><body><h1>Error: One or more sensors are disconnected!</h1></body></html>"));
      } else {
        // Calcular diferencia de temp
        float tempDiff = abs(tempSurface - tempDeep);

        // Controlar el actuador con la humedad y diderencia de temperatura 
        if (tempDiff > TEMP_DIFF_THRESHOLD || dhtHumidity > HUMIDITY_THRESHOLD) {
          digitalWrite(OUTPUT_PIN, HIGH);
        } else {
          digitalWrite(OUTPUT_PIN, LOW);
        }

        // Contenido pagina web
        String htmlResponse = F("<html><body>");
        htmlResponse += F("<h1>Sensor Readings</h1>");
        htmlResponse += "<p>Surface Temperature: " + String(tempSurface) + " C</p>";
        htmlResponse += "<p>Deep Temperature: " + String(tempDeep) + " C</p>";
        htmlResponse += "<p>Temperature Difference: " + String(tempDiff) + " C</p>";
        htmlResponse += "<p>DHT11 Temperature: " + String(dhtTemp) + " C</p>";
        htmlResponse += "<p>DHT11 Humidity: " + String(dhtHumidity) + " %</p>";
        htmlResponse += "<p>Pump Status: " + String(digitalRead(OUTPUT_PIN) == HIGH ? "ON" : "OFF") + "</p>";
        htmlResponse += F("</body></html>");

        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Content-type:text/html"));
        client.println();
        client.println(htmlResponse);
      }
    }

    // Cierra conexion
    client.stop();
  }
}
