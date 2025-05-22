// NeonPostgresOverHTTP - https://github.com/neondatabase-labs/NeonPostgresOverHTTP
// Copyright © 2025, Peter Bendel
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is an example that uses a DHT20 temperature and humidity sensor connected to the I2C bus.
// It also uses a LED connected to digital port D2.
//
// Sensor:
// ---------------------------------------------------------------------------------------
// It shows how to store real sensor values' history in a Postgres database(from DHT20 sensor).
// Actuator:
// ---------------------------------------------------------------------------------------
// It also shows how to retrieve the last value of a LED state from the database and set the LED accordingly.
// This can be used to control the LED from a remote location by sending a command to the database.
// The LED state is confirmed back to the database once the command is applied.
//
// In your postgres database you can switch on and off the LED on the microcontroller by executing the following command:
//
// -- switch the LED off
// INSERT INTO actorvalues (actor_name, actor_value)
// VALUES ('led', 'off');
// -- switch the LED on
// INSERT INTO actorvalues (actor_name, actor_value)
// VALUES ('led', 'on');
//
//
// Prerequiisites:
// ---------------------------------------------------------------------------------------
// This example was tested with the following hardware and software:
// Hardware:
// - Arduino RP2040 Connect board
//    https://docs.arduino.cc/hardware/nano-rp2040-connect/
// - Grove shield for Arduino Nano with the following modification:
//    - https://wiki.seeedstudio.com/Grove_Shield_for_Arduino_Nano/
//    - https://forum.seeedstudio.com/t/grove-shield-for-arduino-nano-v1-0-can-it-be-used-with-a-nano-rp2040/258699?u=bodobolero
// - Grove Temperature and Humidity Sensor DHT20 connected to I2C port 
//    https://wiki.seeedstudio.com/Grove-Temperature-Humidity-Sensor-DH20/
// - Grove LED connected to digital port D2
//    https://wiki.seeedstudio.com/Grove-Red_LED/
// Software:
// - Arduino IDE 2.3.6 or later
// - Board Manager: Arduino Mbed OS Nano boards -> Arduino RP2040 Connect
// - Library Manager:
//   - WifiNINA by Arduino version 1.9.1
//     https://docs.arduino.cc/libraries/wifinina/
//   - ArduinoJson by Benoit Blanchon version 7.4.1
//    https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
//   - Grove Temperature and Humidity Sensor DHT20 by Seeed Studio version 2.0.2
//    https://github.com/Seeed-Studio/Grove_Temperature_And_Humidity_Sensor
// ---------------------------------------------------------------------------------------------------------


#include <SPI.h>
#include <WiFiNINA.h> // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <NeonPostgresOverHTTP.h>

#include "Wire.h" // I2C library
#include "DHT.h" // DHT 20 humidity and temperature sensor library
#define DHTTYPE DHT20  // DHT 20 (Temperature and Humidity sensor)

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;
NeonPostgresOverHTTPProxyClient sqlClient(client, DATABASE_URL, NEON_PROXY);

const char* createSensors = R"SQL(
CREATE TABLE if not exists sensorvalues(
  measure_time timestamp without time zone DEFAULT now() NOT NULL,
  sensor_name text NOT NULL, sensor_value float, 
  PRIMARY KEY (sensor_name, measure_time)
)
)SQL";

const char* createActors = R"SQL(
CREATE TABLE if not exists actorvalues (
    sent_time timestamp without time zone DEFAULT now() NOT NULL,
    acknowledge_time timestamp without time zone,
    actor_name text NOT NULL, actor_value text NOT NULL,
    PRIMARY KEY (actor_name, sent_time)
);
)SQL";

const char* insertSensorValue = R"SQL(
INSERT INTO SENSORVALUES (sensor_name, sensor_value) VALUES ($1::text, $2::float)
)SQL";

const char* queryActorValue = R"SQL(
SELECT actor_value, sent_time
FROM actorvalues
WHERE actor_name = $1::text
ORDER BY sent_time DESC
LIMIT 1;
)SQL";

const char* confirmActorValue = R"SQL(
UPDATE actorvalues
SET acknowledge_time = now()
WHERE actor_name = $1::text AND sent_time = $2::timestamp;
)SQL";


int counter = 1;
const int ledPin = 2;  // LED pin in digital port D2
DHT dht(DHTTYPE);      // DHT20 don't need to define Pin, uses I2C I/F


void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    // wait 10 seconds before trying to connect
    delay(10000);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
  }
  Serial.println("Connected to WiFi");

  // create table for sensor values
  sqlClient.setQuery(createSensors);
  const char* errorMessage = sqlClient.execute();
  while (errorMessage != nullptr) {
    Serial.println(errorMessage);
    checkAndPrintWiFiStatus();
    errorMessage = sqlClient.execute();
  }
  // create table for actor values
  sqlClient.setQuery(createActors);
  errorMessage = sqlClient.execute();
  while (errorMessage != nullptr) {
    Serial.println(errorMessage);
    checkAndPrintWiFiStatus();
    errorMessage = sqlClient.execute();
  }

  pinMode(ledPin, OUTPUT);  //Sets the led pinMode to Output
  // init Temperature and Humidity sensor
  Wire.begin();
  dht.begin();
}

void loop() {
  // test DHT20
  float temp_hum_val[2] = { 0 };
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (!dht.readTempAndHumidity(temp_hum_val)) {
    Serial.print("Humidity: ");
    Serial.print(temp_hum_val[0]);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(temp_hum_val[1]);
    Serial.println(" Degree Celsius");
  } else {
    Serial.println("Could not read sensor values from DHT20");
  }

  checkAndPrintWiFiStatus();

  // insert temperature and humidity sensor values into postgres database
  Serial.println("\nExecuting insert statement for temperature...");
  sqlClient.setQuery(insertSensorValue);
  JsonArray params = sqlClient.getParams();
  params.clear();
  params.add("temperature");
  params.add(temp_hum_val[1]);
  const char* errorMessage = sqlClient.execute();
  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  }
  Serial.println("\nExecuting insert statement for humidity...");
  // sqlClient.setQuery(insertSensorValue); already done before
  // JsonArray params = sqlClient.getParams(); already done before
  params.clear();
  params.add("humidity");
  params.add(temp_hum_val[0]);
  errorMessage = sqlClient.execute();
  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  }

  // retrieve and print led value and set led accordingly
  Serial.println("\nRetrieving wanted led state...");
  sqlClient.setQuery(queryActorValue);
  params.clear();
  params.add("led");
  errorMessage = sqlClient.execute();
  // // to debug:
  // serializeJson(sqlClient.getRawJsonResult(), Serial);
  // // or:
  // sqlClient.printRawJsonResult(Serial);
  if (errorMessage != nullptr && sqlClient.getRowCount() == 1) {
    Serial.println(errorMessage);
  } else {

    JsonArray rows = sqlClient.getRows();
    for (JsonObject row : rows) {
      const char* actor_value = row["actor_value"];  // "on" or "off"
      const char* sent_time = row["sent_time"];      // request timestamp
      Serial.print("led state: ");
      Serial.print(actor_value);
      Serial.print(" requested at: ");
      Serial.println(sent_time);
      if (strcmp(actor_value, "on") == 0) {
        // actor_value is "on"
        digitalWrite(ledPin, HIGH);  //Sets the led voltage to high/on
      } else {
        // actor_value is "off"
        digitalWrite(ledPin, LOW);   //Sets the led voltage to low/off
      }
      // now confirm we have received the message to set the led state back to the sender
      Serial.println("\nConfirming we have received and updated led state...");
      sqlClient.setQuery(confirmActorValue);
      params.clear();
      params.add("led");
      params.add(sent_time);
      errorMessage = sqlClient.execute();
      // // to debug:
      // serializeJson(sqlClient.getRawJsonResult(), Serial);
      // // or:
      // sqlClient.printRawJsonResult(Serial);
      if (errorMessage != nullptr && sqlClient.getRowCount() == 1) {
        Serial.println(errorMessage);
      }
    }
  }
  //wait a few seconds before next request
  delay(8000);
}


void checkAndPrintWiFiStatus() {
  status = WiFi.status();
  while (status != WL_CONNECTED) {
    WiFi.disconnect();
    // wait 10 seconds before trying to connect again
    delay(10000);
    Serial.print("Lost connection - attempting to re-connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
  }
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
