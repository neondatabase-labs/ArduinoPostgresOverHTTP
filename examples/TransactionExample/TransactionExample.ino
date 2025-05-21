// NeonPostgresOverHTTP example - https://github.com/neondatabase-labs/NeonPostgresOverHTTP
// Copyright © 2025, Peter Bendel and neondatabase labs (neon.tech)
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It shows how to execute multiple SQL statements in a single transaction
// ---------------------------------------------------------------------------------------------------------

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
CREATE TABLE IF NOT EXISTS accounts (
    customer_id BIGINT PRIMARY KEY,
    balance DOUBLE PRECISION
);
)SQL";

const char* truncate = R"SQL( 
truncate accounts;
)SQL";

const char* insert = R"SQL(
INSERT INTO accounts (customer_id, balance) VALUES
(1, 1000.50),
(2, 2500.75);
)SQL";

const char* txn_stmt_1 = R"SQL(
UPDATE accounts
SET balance = balance - $1::DOUBLE PRECISION
WHERE customer_id = 2;
)SQL";

const char* txn_stmt_2 = R"SQL(
UPDATE accounts
SET balance = balance + $1::DOUBLE PRECISION
WHERE customer_id = 1;
)SQL";

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

  // create table accounts
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
  Serial.println("\nDeleting all data from table accounts...");
  sqlClient.setQuery(truncate);
  const char* errorMessage = sqlClient.execute();
  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  }

  Serial.println("\nInserting two accounts into accounts table...");
  sqlClient.setQuery(insert);
  errorMessage = sqlClient.execute();
  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
  }

  // transfer amount of 100.0 from account 2 to account 1 in a single transaction
  // that executes or fails both associated statements atomically 
  Serial.println("\nTransferring 100.0 from account 2 to account 1 in a single transaction...");
  sqlClient.startTransaction();
  // remove 100.0 from account 2
  sqlClient.addQueryToTransaction(txn_stmt_1);
  JsonArray params1 = sqlClient.getParamsForTransactionQuery(0);
  params1.clear();
  params1.add(100.0);
  // add 100.0 to account 1
  sqlClient.addQueryToTransaction(txn_stmt_2);
  JsonArray params2 = sqlClient.getParamsForTransactionQuery(1);
  params2.clear();
  params2.add(100.0);
  // execute transaction with both statements
  errorMessage = sqlClient.executeTransaction();
  // // to debug:
  // serializeJson(sqlClient.getRawJsonResultForTransaction(), Serial);
  // // or:
  // sqlClient.printRawJsonResultForTransaction(Serial);
  Serial.print("\nThe first statement in the transaction updated ");
  Serial.print(sqlClient.getRowsForTransactionQuery(0));
  Serial.println(" row(s)");

  Serial.print("\nThe second statement in the transaction updated ");
  Serial.print(sqlClient.getRowsForTransactionQuery(1));
  Serial.println(" row(s)");

  if (errorMessage != nullptr) {
    Serial.println(errorMessage);
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
