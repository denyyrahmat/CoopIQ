#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "HX711.h"
#include <ESP32Servo.h>

// WiFi credentials
const char *ssid = "POWER RANGER";
const char *password = "bebaslepas00";

// MQTT broker credentials
const char *MQTT_username = "";
const char *MQTT_password = "";
const char *mqtt_server = "192.168.0.111";

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Sensor and actuator pins
#define DHTTYPE DHT11
const int DHT_PIN = 13;
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
const int MQ135_SENSOR_PIN = 34; // Analog pin
const int SERVO_PIN = 14;

// Initialize sensors
DHT dht(DHT_PIN, DHTTYPE);
HX711 scale;
Servo servo;

// Timer variables
long lastMeasure = 0;
const int sensitivity = 200; // Adjust this value based on your calibration

// Interpret air quality
String interpret_air_quality(int sensor_value)
{
  if (sensor_value < 50)
    return "Excellent";
  else if (sensor_value < 100)
    return "Good";
  else if (sensor_value < 150)
    return "Moderate";
  else if (sensor_value < 200)
    return "Poor";
  else
    return "Dangerous";
}

// WiFi connection setup
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT callback function
void callback(String topic, byte *message, unsigned int length)
{
  // Handle MQTT messages if needed
  // Currently, we don't have any specific actions in callback for automatic servo control.
}

// MQTT reconnect function
void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", MQTT_username, MQTT_password))
    {
      Serial.println("connected");
      // Subscribe to MQTT topics if needed
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  dht.begin();
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(420.5898); // scale factor input here
  scale.tare();              // reset scale

  servo.attach(SERVO_PIN);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMeasure > 5000)
  { // Adjust the interval as needed
    lastMeasure = now;

    float humidity = dht.readHumidity();
    float temperatureC = dht.readTemperature();
    if (isnan(humidity) || isnan(temperatureC))
    {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    float weight = scale.get_units(10);
    int sensor_value = analogRead(MQ135_SENSOR_PIN);
    int air_quality = sensor_value * sensitivity / 1023;
    String air_quality_label = interpret_air_quality(air_quality);

    // Publish sensor data to MQTT
    client.publish("coop/humidity", String(humidity).c_str());
    client.publish("coop/temperature", String(temperatureC).c_str());
    client.publish("coop/load", String(weight).c_str());
    client.publish("coop/gas", String(air_quality).c_str());

    // Print sensor data to Serial
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" ºC");
    Serial.print("Weight: ");
    Serial.println(weight);
    Serial.print("Air Quality Index (Calibrated): ");
    Serial.println(air_quality);
    Serial.print("Air Quality: ");
    Serial.println(air_quality_label);
    Serial.println("-----------------------------------");

    // Automatic servo control based on weight
    if (weight < 200)
    {
      for (int posDegrees = 0; posDegrees <= 90; posDegrees++)
      {
        servo.write(posDegrees);
        // delay(15);
      }
    }
    else if (weight > 1500)
    {
      for (int posDegrees = 90; posDegrees >= 0; posDegrees--)
      {
        servo.write(posDegrees);
        // delay(15);
      }
    }
  }
}
