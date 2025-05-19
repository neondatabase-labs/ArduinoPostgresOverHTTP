// NeonPostgresOverHTTP example - https://github.com/neondatabase-labs/NeonPostgresOverHTTP
// Copyright © 2025, Peter Bendel and neondatabase labs (neon.tech)
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It simulates a sensor value by incrementing a counter and stores the counter value in a Postgres database.
// ---------------------------------------------------------------------------------------------------------

// Example output in serial monitor:
// Attempting to connect to SSID: YourSSID
// Connected to WiFi

// Executing create table (if not exists) statement in Postgres database...
// SSID: YourSSID
// IP Address: 192.168.179.16
// signal strength (RSSI):-46 dBm

// Executing insert statement...

// IP Address: 192.168.179.16
// signal strength (RSSI):-45 dBm

// Executing insert statement...

// Retrieving last 3 sensor values...
// Rowcount: 1
// Time: 2025-05-19 13:14:55.919374 Sensor: counter Value: 1.00
// SSID: YourSSID
// IP Address: 192.168.179.16
// signal strength (RSSI):-45 dBm

// Executing insert statement...

// Retrieving last 3 sensor values...
// Rowcount: 2
// Time: 2025-05-19 13:15:10.350916 Sensor: counter Value: 2.00
// Time: 2025-05-19 13:14:55.919374 Sensor: counter Value: 1.00
// SSID: YourSSID
// IP Address: 192.168.179.16
// signal strength (RSSI):-46 dBm

// Time: 2025-05-19 13:15:24.687117 Sensor: counter Value: 3.00
// Time: 2025-05-19 13:15:10.350916 Sensor: counter Value: 2.00
// Time: 2025-05-19 13:14:55.919374 Sensor: counter Value: 1.00
// SSID: YourSSID
// IP Address: 192.168.179.16
// signal strength (RSSI):-46 dBm
// ...

#include <SPI.h>
#include <WiFiNINA.h> // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <NeonPostgresOverHTTP.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;
NeonPostgresOverHTTPProxyClient sqlClient(client, DATABASE_URL, NEON_PROXY);

const char* create = R"SQL(
CREATE TABLE if not exists sensorvalues(
  measure_time timestamp without time zone DEFAULT now() NOT NULL,
  sensor_name text NOT NULL, sensor_value float, PRIMARY KEY (sensor_name, measure_time)
)
)SQL";

const char* insert = R"SQL(
INSERT INTO SENSORVALUES (sensor_name, sensor_value) VALUES ('counter', $1::int)
)SQL";

const char* query = R"SQL(
SELECT sensor_name, measure_time, sensor_value
FROM sensorvalues
WHERE sensor_name = 'counter'
  AND measure_time >= now() - interval '1 minutes'
ORDER BY measure_time DESC LIMIT 3;
)SQL";


int counter = 1;

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
  Serial.println("\nExecuting create table (if not exists) statement in Postgres database...");
  sqlClient.setQuery(create);
  const char* errorMessage = sqlClient.execute();
  while (errorMessage != nullptr) {
    Serial.println(errorMessage);
    checkAndPrintWiFiStatus();
    errorMessage = sqlClient.execute();
  }
}

void loop() {
  checkAndPrintWiFiStatus();
  Serial.println("\nExecuting insert statement...");
  sqlClient.setQuery(insert);
  JsonArray params = sqlClient.getParams();
  params.clear();
  params.add(counter++);
  const char* errorMessage = sqlClient.execute();
  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  }

  // retrieve and print last 3 values in database
  Serial.println("\nRetrieving last 3 sensor values...");
  sqlClient.setQuery(query);
  params.clear();
  errorMessage = sqlClient.execute();
  // // to debug:
  // serializeJson(sqlClient.getRawJsonResult(), Serial);
  // // or:
  // sqlClient.printRawJsonResult(Serial);

  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  } else {
    Serial.print("Rowcount: ");
    Serial.println(sqlClient.getRowCount());
    JsonArray rows = sqlClient.getRows();
    for (JsonObject row : rows) {
      const char* measure_time = row["measure_time"];  // "2025-01-01 09:54:26.966508", ...
      const char* sensor_name = row["sensor_name"];    // "counter"
      float sensor_value = row["sensor_value"];
      Serial.print("Time: ");
      Serial.print(measure_time);
      Serial.print(" Sensor: ");
      Serial.print(sensor_name);
      Serial.print(" Value: ");
      Serial.println(sensor_value);
    }
  }
  //wait 10 seconds before next request
  delay(10000);
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
