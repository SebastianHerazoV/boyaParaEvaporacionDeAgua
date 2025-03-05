#include <WiFi.h>
#include <ESPmDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>     

// ----- Pin Definitions -----
#define ONE_WIRE_BUS 5      
#define OUTPUT_PIN 4      
#define DHT_PIN 13        

// ----- Access Point Credentials -----
const char* ap_ssid = "A58SEBAS";        // Name of the ESP32's Wi-Fi network
const char* ap_password = "1101876738";    // Password for the ESP32's Wi-Fi network

// ----- Create Objects -----
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DHT dht(DHT_PIN, DHT11); // Initialize DHT11 sensor

// ----- Variables & Thresholds -----
DeviceAddress sensorSurface;  // Address for surface temperature sensor
DeviceAddress sensorDeep;     // Address for deep temperature sensor
const float TEMP_DIFF_THRESHOLD = 2.0;  // Temperature difference threshold in °C
const float HUMIDITY_THRESHOLD = 70.0;  // Humidity threshold in %

WiFiServer server(80); // Create a web server on port 80

void setup() {
  // Initialize serial communication (optional, if you can use a data-capable cable later)
  Serial.begin(115200);

  // Initialize temperature sensors
  tempSensors.begin();

  // Retrieve sensor addresses
  if (!tempSensors.getAddress(sensorSurface, 0)) {
    Serial.println("Unable to find address for DS18B20 sensor 0 (surface).");
    while (1); // Halt if the surface sensor is not found
  }
  if (!tempSensors.getAddress(sensorDeep, 1)) {
    Serial.println("Unable to find address for DS18B20 sensor 1 (deep).");
    while (1); // Halt if the deep sensor is not found
  }

  // Initialize DHT11 sensor
  dht.begin();
  Serial.println("DHT11 sensor initialized.");

  // Set up output pin
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW); // Ensure the output pin is initially off

  // Start the ESP32's Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point started. Connect to Wi-Fi SSID: ");
  Serial.println(ap_ssid);

  // Print the ESP32's IP address
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP Address: ");
  Serial.println(IP);

  // Start mDNS
  if (!MDNS.begin("esp")) { // Set the hostname to "esp"
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started. Access the ESP32 at http://esp.local");
  }

  server.begin(); // Start the web server
}

void loop() {
  // Check for incoming client connections
  WiFiClient client = server.available();
  if (client) {
    String currentLine = ""; // Store incoming HTTP requests
    bool requestComplete = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // Read one byte from the client
        if (c == '\n') { // End of HTTP request line
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
      // Request temperature readings from the DS18B20 sensors
      tempSensors.requestTemperatures();
      float tempSurface = tempSensors.getTempC(sensorSurface);
      float tempDeep = tempSensors.getTempC(sensorDeep);

      // Request temperature and humidity readings from the DHT11 sensor
      float dhtTemp = dht.readTemperature(); // Temperature in Celsius
      float dhtHumidity = dht.readHumidity(); // Humidity in %

      // Check for disconnected sensors
      if (tempSurface == DEVICE_DISCONNECTED_C || tempDeep == DEVICE_DISCONNECTED_C ||
          isnan(dhtTemp) || isnan(dhtHumidity)) {
        client.println(F("<html><body><h1>Error: One or more sensors are disconnected!</h1></body></html>"));
      } else {
        // Calculate the absolute temperature difference
        float tempDiff = abs(tempSurface - tempDeep);

        // Control the output pin based on temperature difference or humidity
        if (tempDiff > TEMP_DIFF_THRESHOLD || dhtHumidity > HUMIDITY_THRESHOLD) {
          digitalWrite(OUTPUT_PIN, HIGH);
        } else {
          digitalWrite(OUTPUT_PIN, LOW);
        }

        // Build the webpage content
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

    // Close the connection
    client.stop();
  }
}
