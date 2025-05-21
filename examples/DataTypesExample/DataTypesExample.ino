// NeonPostgresOverHTTP example - https://github.com/neondatabase-labs/NeonPostgresOverHTTP
// Copyright © 2025, Peter Bendel and neondatabase labs (neon.tech)
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It shows how to use the most important Postgres datatypes with this library.
// ---------------------------------------------------------------------------------------------------------
// Example output:
// Integer: SELECT $1::INTEGER as val1, $2::INTEGER as val2
// val1: 123

// Text: SELECT $1::TEXT as val1, $2::TEXT as val2
// val1: hello

// Boolean: SELECT $1::BOOLEAN as val1, $2::BOOLEAN as val2
// val1: true

// Float: SELECT $1::FLOAT as val1, $2::FLOAT as val2
// val1: 3.140000

// Text Array: SELECT $1::TEXT[] as val1
// val1 text array elements:
// alpha
// beta
// gamma

// Jsonb: SELECT $1::JSONB as val1
// val1.a: 1
// val1.b: true

#include <SPI.h>
#include <WiFiNINA.h>         // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h"  // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
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
}

void loop() {
  checkAndPrintWiFiStatus();
  JsonArray params = sqlClient.getParams();
  const char* err = nullptr;


  {  // INTEGER
    Serial.println("\nInteger: SELECT $1::INTEGER as val1, $2::INTEGER as val2");
    sqlClient.setQuery("SELECT $1::INTEGER as val1, $2::INTEGER as val2");
    params.clear();
    // specify parameter as integer string literal
    params.add("123");
    // specify paramater as int
    const int parm2 = 4711;
    params.add(parm2);

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        // retrieve INTEGER into int
        int val1 = row["val1"].as<int>();
        Serial.print("val1: ");
        Serial.println(val1);
      }
    }
  }

  {  // TEXT
    Serial.println("\nText: SELECT $1::TEXT as val1, $2::TEXT as val2");
    sqlClient.setQuery("SELECT $1::TEXT as val1, $2::TEXT as val2");
    params.clear();
    // specify parameter as text string literal
    params.add("hello");
    // specify parameter as C++ const char*
    const char* parm2 = "world";
    params.add(parm2);

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        const char* val1 = row["val1"];  // as string
        Serial.print("val1: ");
        Serial.println(val1);
      }
    }
  }

  {  // BOOLEAN
    Serial.println("\nBoolean: SELECT $1::BOOLEAN as val1, $2::BOOLEAN as val2");
    sqlClient.setQuery("SELECT $1::BOOLEAN as val1, $2::BOOLEAN as val2");
    params.clear();
    // specify parameter as boolean string literal
    params.add("true");
    // specify parameter as C++ bool
    bool parm2 = false;
    params.add(parm2);

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        bool val1 = row["val1"].as<bool>();
        Serial.print("val1: ");
        Serial.println(val1 ? "true" : "false");
      }
    }
  }

  {  // FLOAT
    Serial.println("\nFloat: SELECT $1::FLOAT as val1, $2::FLOAT as val2");
    sqlClient.setQuery("SELECT $1::FLOAT as val1, $2::FLOAT as val2");
    params.clear();
    // specify parameter as float string literal
    params.add("3.14");
    // specify parameter as C++ float
    float parm2 = 2.718f;
    params.add(parm2);

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        float val1 = row["val1"].as<float>();
        Serial.print("val1: ");
        Serial.println(val1, 6);
      }
    }
  }

  {  // TEXT ARRAY
    Serial.println("\nText Array: SELECT $1::TEXT[] as val1");
    sqlClient.setQuery("SELECT $1::TEXT[] as val1");
    params.clear();
    // specify parameter as array literal
    params.add("{alpha,beta,gamma}");

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        JsonArray val1Array = row["val1"].as<JsonArray>();  // parsed array
        Serial.println("val1 text array elements:");
        for (const char* element : val1Array) {
          Serial.println(element);
        }
      }
    }
  }

  {  // JSONB
    Serial.println("\nJsonb: SELECT $1::JSONB as val1");
    sqlClient.setQuery("SELECT $1::JSONB as val1");
    params.clear();
    // specify parameter as JSONB string
    params.add("{\"a\":1,\"b\":true}");

    err = sqlClient.execute();
    if (err) {
      Serial.print("Error: ");
      Serial.println(err);
    } else {
      JsonArray rows = sqlClient.getRows();
      for (JsonObject row : rows) {
        JsonObject val1json = row["val1"].as<JsonObject>();
        int a = val1json["a"].as<int>();
        bool b = val1json["b"].as<bool>();
        Serial.print("val1.a: ");
        Serial.println(a);
        Serial.print("val1.b: ");
        Serial.println(b ? "true" : "false");
      }
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
  Serial.print("\nSSID: ");
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
