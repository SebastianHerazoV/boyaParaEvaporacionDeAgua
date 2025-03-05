#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>

// ----- Pin Definitions -----
#define ONE_WIRE_BUS 4      // DS18B20 data pin
#define PUMP_PIN 15         // Digital output controlling the water pump

// ----- Wi-Fi Credentials -----
const char* ssid = "ALEXA";        // Replace with your Wi-Fi SSID
const char* password = "aLexa3101."; // Replace with your Wi-Fi password

// ----- Create Objects -----
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

// ----- Variables & Thresholds -----
DeviceAddress sensorSurface;  // Address for surface temperature sensor
DeviceAddress sensorDeep;     // Address for deep temperature sensor
const float tempDiffThreshold = 4.0;  // Temperature difference threshold in °C

WiFiServer server(80); // Create a web server on port 80

void setup() {
  Serial.begin(115200);
  tempSensors.begin();

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);  // Ensure pump is initially off

  // Retrieve sensor addresses
  if (!tempSensors.getAddress(sensorSurface, 0)) {
    Serial.println("Unable to find address for DS18B20 sensor 0 (surface).");
    while (1);
  }
  if (!tempSensors.getAddress(sensorDeep, 1)) {
    Serial.println("Unable to find address for DS18B20 sensor 1 (deep).");
    while (1);
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Print the ESP32's IP address

  server.begin(); // Start the web server
}

void loop() {
  // Check for incoming client connections
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected.");
    String currentLine = ""; // Store incoming HTTP requests

    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // Read one byte from the client
        if (c == '\n') { // End of HTTP request
          // Send a webpage with sensor readings
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();

          // Read sensor data
          tempSensors.requestTemperatures();
          float tempSurface = tempSensors.getTempC(sensorSurface);
          float tempDeep = tempSensors.getTempC(sensorDeep);

          // Check for invalid temperature readings
          if (tempSurface == DEVICE_DISCONNECTED_C || tempDeep == DEVICE_DISCONNECTED_C) {
            client.println("<html><body><h1>Error: Temperature sensor disconnected!</h1></body></html>");
            break;
          }

          // Calculate the temperature difference (surface - deep)
          float tempDiff = tempSurface - tempDeep;

          // Control the pump based on temperature difference
          if (tempDiff > tempDiffThreshold) {
            digitalWrite(PUMP_PIN, HIGH);
          } else {
            digitalWrite(PUMP_PIN, LOW);
          }

          // Build the webpage content
          client.println("<html><body>");
          client.println("<h1>Sensor Readings</h1>");
          client.print("<p>Surface Temperature: ");
          client.print(tempSurface);
          client.println(" C</p>");
          client.print("<p>Deep Temperature: ");
          client.print(tempDeep);
          client.println(" C</p>");
          client.print("<p>Temperature Difference: ");
          client.print(tempDiff);
          client.println(" C</p>");
          client.print("<p>Pump Status: ");
          client.print(digitalRead(PUMP_PIN) == HIGH ? "ON" : "OFF");
          client.println("</p>");
          client.println("</body></html>");

          break;
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
  }
}