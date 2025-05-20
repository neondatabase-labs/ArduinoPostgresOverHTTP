# NeonPostgresOverHTTP library

NeonPostgresOverHTTP is a library for Arduino and Arduino-compatible micro-controllers that allows you to connect to a PostgreSQL database over HTTP.

This can be used to store sensor data directly into a PostgreSQL database without a middleman server like home assistant.
You can also read values from the database and use them to control actuators.

This library is designed to work with the ArduinoJson library, which is used to parse JSON data from the PostgreSQL database.

<!-- toc -- [NeonPostgresOverHTTP library](#neonpostgresoverhttp-library) -->
- [NeonPostgresOverHTTP library](#neonpostgresoverhttp-library)
  - [Important note](#important-note)
  - [Usage overview](#usage-overview)
  - [Prerequisites](#prerequisites)
    - [PostgreSQL](#postgresql)
    - [Wifi](#wifi)
    - [ArduinoJson library](#arduinojson-library)
  - [API Getting Started](#api-getting-started)
    - [Provisioning a PostgreSQL database on Neon and getting the connection URL and Neon proxy hostname](#provisioning-a-postgresql-database-on-neon-and-getting-the-connection-url-and-neon-proxy-hostname)
    - [Provisioning the Neon Proxy with your own PostgreSQL database](#provisioning-the-neon-proxy-with-your-own-postgresql-database)
    - [Establishing a database connection](#establishing-a-database-connection)
    - [Executing a single query](#executing-a-single-query)
    - [Run a SQL statement with parameters](#run-a-sql-statement-with-parameters)
    - [Retrieving a result set](#retrieving-a-result-set)
    - [Running multiple statements in a transaction](#running-multiple-statements-in-a-transaction)
    - [Error handling and debugging](#error-handling-and-debugging)
    - [Reading and writing different PostgreSQL data types](#reading-and-writing-different-postgresql-data-types)
<!-- /toc -->

## Important note

To allow different Wifi client implementations the library does not define a dependency on a specific Wifi client library.
Instead you need to manually install a Wifi client library that implements the `WiFiClient` interface that matches your microcontroller.
This library has been tested with the following Wifi client libraries:
- [WiFiNINA](https://docs.arduino.cc/libraries/wifinina/)  e.g. for Arduino RP2040 Connect
- [Espressif ESP32 Wifi library](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi) - tested with XIAO ESP32C6
  - see [examples/BasicExample_ESP32C6](examples/BasicExample_ESP32C6) with XIAO ESP32C6

## Usage overview

```C
...
#include <WiFiNINA.h>
#include <NeonPostgresOverHTTP.h>
...
WiFiSSLClient client;
NeonPostgresOverHTTPProxyClient sqlClient(client, DATABASE_URL, NEON_PROXY);

const char* create = R"SQL(
CREATE TABLE if not exists sensorvalues(
  measure_time timestamp without time zone DEFAULT now() NOT NULL,
  sensor_name text NOT NULL, sensor_value float, PRIMARY KEY (sensor_name, measure_time)
)
)SQL";


int counter = 1;
int status = WL_IDLE_STATUS;

void setup() {
    Serial.begin(9600);

    while (status != WL_CONNECTED) {
        Serial.print("Connecting to WiFi...");
        status = WiFi.begin(SSID, PASSWORD);
        delay(10000);
    }
    Serial.println("Connected to WiFi");

    // create table for sensor values
    sqlClient.setQuery(create);
    const char* errorMessage = sqlClient.execute();
    while (errorMessage != nullptr) {
        Serial.println(errorMessage);
        checkAndPrintWiFiStatus();
        errorMessage = sqlClient.execute();
    }
}

const char* insert = R"SQL(
INSERT INTO SENSORVALUES (sensor_name, sensor_value) VALUES ('counter', $1::int)
)SQL";

void loop() {
    Serial.println("\nExecuting insert statement...");
    sqlClient.setQuery(insert);
    JsonArray params = sqlClient.getParams();
    params.clear();
    params.add(counter++);
    const char* errorMessage = sqlClient.execute();
    if (errorMessage != nullptr) {
        Serial.println(errorMessage);
    }
}
```

See the [examples](examples) folder for more examples.

## Prerequisites

### PostgreSQL
You need a PostgreSQL database with the following extensions installed:

- Neon Proxy which supports query over HTTPS with Json payload

You can use a database by Neon (https://neon.tech/) which is a serverless PostgreSQL database as a service with Neon Proxy support.

You can also deploy the [open source Neon Proxy](https://github.com/neondatabase/neon/blob/main/proxy/README.md#sql-over-http) on your own server or with other cloud providers.

See [DeployNeonProxyWithYourOwnPostgres.md](DeployNeonProxyWithYourOwnPostgres.md) for instructions on how to deploy the Neon Proxy with your own PostgreSQL database.

**Rationale:**

Normally PostgreSQL servers only accept TCP connections from clients like psql or libraries like libpq that speak the Postgres Wire Protocol.
I am not aware of any library implementation for microcontrollers that implements the Postgres Wire Protocol client.

Thus we use a lightweight HTTP proxy that accepts HTTPS requests and translates them into Postgres Wire Protocol requests.
Thus we "only" need a TLS client library and JSON generator/parser library to communicate with the proxy.

### Wifi
You need a microontroller fast enough to run an SSL client (like BearSSL) and a Wifi client (like WiFiNINA or Espressif ESP32 Wifi library).

Your Wifi controller must have a supported library that implements the WiFiClient.h WifiClient interface

```C
// WifiClient.h
class WiFiClient : public Client {
    ...
}
```

for example 
- Arduino WifiNINA https://docs.arduino.cc/libraries/wifinina/
- [Espressif ESP32 Wifi library](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h)

### ArduinoJson library

This library defines a dependency on the ArduinoJson library which is automatically installed by the Arduino IDE.
You can also install a specific version >=  7.3 of the library manually from the Arduino IDE Library Manager.

This library is used to create and parse the JSON payloads that are sent to and received from the PostgreSQL database's Neon proxy.

## API Getting Started

See [src/NeonPostgresOverHTTP.h](src/NeonPostgresOverHTTP.h) for the latest API reference.

### Provisioning a PostgreSQL database on Neon and getting the connection URL and Neon proxy hostname

Signing up for a free Neon account (no credit card required) is easy. Just go to https://neon.tech and click on "Start for free".
For more documentation see https://neon.tech/docs/get-started-with-neon/signing-up

In Neon every database is part of a project.
Neon projects are created in a specicific region (a location in the cloud). 
Follow the instructions to create your first project and select the region (location) closest to your microcontroller's Wifi network.

Once you have created a project you can retrieve the connection URL, see
https://neon.tech/docs/get-started-with-neon/connect-neon#obtaining-connection-details

example of a connection URL: `postgresql://neondb_owner:password@ep-quiet-wildcat-a2l67iq9-pooler.eu-central-1.aws.neon.tech/neondb?sslmode=require`

In addition you need the Neon proxy hostname for your region.

example of a Neon proxy hostname: `"api.eu-central-1.aws.neon.tech"`

The proxy hostname is different for each region and the pattern is `api.<region>.aws.neon.tech`.

Here is a table that helps you find the correct Neon proxy hostname for your region:

| Region | Neon Proxy Hostname |
|--------|---------------------|
| us-east-1 | api.us-east-1.aws.neon.tech |
| us-west-2 | api.us-west-2.aws.neon.tech |
| eu-central-1 | api.eu-central-1.aws.neon.tech |
| ... | ... |

and so on

### Provisioning the Neon Proxy with your own PostgreSQL database

See [DeployNeonProxyWithYourOwnPostgres.md](DeployNeonProxyWithYourOwnPostgres.md) for instructions on how to deploy the Neon Proxy with your own PostgreSQL database.

The following example was tested with Upstream PostgreSQL 16.0 and Neon Proxy built locally from source.
[examples/BasicExample_ESP32C6_with_upstream_postgres](examples/BasicExample_ESP32C6_with_upstream_postgres)

### Establishing a database connection

Copy [arduino_secrets.h.example](arduino_secrets.h.example) to arduino_secrets.h and fill in the values for your Wifi network and PostgreSQL database connection.

```C
#include <WiFiNINA.h>
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <NeonPostgresOverHTTP.h>
...
WiFiSSLClient client;
NeonPostgresOverHTTPProxyClient sqlClient(client, DATABASE_URL, NEON_PROXY);

char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;

void setup() {
    while (status != WL_CONNECTED) {
        status = WiFi.begin(SSID, PASSWORD);
        delay(10000);
    }
}

```

todo

### Executing a single query

```C
const char* insert = R"SQL(
INSERT INTO SENSORVALUES (sensor_name, sensor_value) VALUES ('counter', 1:int)
)SQL";

sqlClient.setQuery(insert);
// following two lines optional, only if params have been used before
// otional: JsonArray params = sqlClient.getParams(); 
// optional: params.clear(); 
const char* errorMessage = sqlClient.execute();
if (errorMessage != nullptr) {
    Serial.println(errorMessage);
}
```

### Run a SQL statement with parameters

```C
const char* insertSensorValue = R"SQL(
INSERT INTO SENSORVALUES (sensor_name, sensor_value) VALUES ($1::text, $2::float)
)SQL";

// example uses #include "DHT.h" from https://github.com/Seeed-Studio/Grove_Temperature_And_Humidity_Sensor
float temp_hum_val[2] = { 0 };
dht.readTempAndHumidity(temp_hum_val);

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
```

### Retrieving a result set

```C
const char* query = R"SQL(
SELECT sensor_name, measure_time, sensor_value
FROM sensorvalues
WHERE sensor_name = 'counter'
  AND measure_time >= now() - interval '1 minutes'
ORDER BY measure_time DESC LIMIT 3;
)SQL";

Serial.println("\nRetrieving last 3 sensor values...");
  sqlClient.setQuery(query);
  params.clear();
  errorMessage = sqlClient.execute();

  if (errorMessage == nullptr) {
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
```

### Running multiple statements in a transaction

todo

### Error handling and debugging

```C
const char* query = R"SQL(
SELECT sensor_name, measure_time, sensor_value
FROM sensorvalues
WHERE sensor_name = 'counter'
  AND measure_time >= now() - interval '1 minutes'
ORDER BY measure_time DESC LIMIT 3;
)SQL";

Serial.println("\nRetrieving last 3 sensor values...");
  sqlClient.setQuery(query);
  params.clear();
  errorMessage = sqlClient.execute();
  // to debug:
  serializeJson(sqlClient.getRawJsonResult(), Serial);
  // or:
  sqlClient.printRawJsonResult(Serial);

  if (errorMessage != nullptr) {
    // to print the error message
    Serial.println(errorMessage);
  } else {
    // process result...
    }
  }
```

### Reading and writing different PostgreSQL data types

todo: examples