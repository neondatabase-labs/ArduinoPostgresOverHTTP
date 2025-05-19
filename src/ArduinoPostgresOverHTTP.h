// ArduinoPostgresOverHTTP example - https://github.com/neondatabase-labs/ArduinoPostgresOverHTTP
// Copyright © 2025, Peter Bendel and neondatabase labs (neon.tech)
// MIT License

#ifndef ARDUINOPOSTGRESOVERHTTPPROXYCLIENT_H
#define ARDUINOPOSTGRESOVERHTTPPROXYCLIENT_H
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include <cstring>

class ArduinoPostgresOverHttpProxyClient {
public:

  /**
     * @brief Constructs an database client that connects over Wifi to a Neon database (https://neon.tech)
     *        @see https://github.com/neondatabase/neon/tree/main/proxy 
     *        Instead of a Neon database you can use any other PostgreSQL database and run the Neon Proxy 
     *        yourself to convert between the SQL over HTTP and the postgres wire protocol.
     *        For details see https://github.com/neondatabase/serverless/blob/main/DEPLOY.md
     * 
     * @param client A Wifi client that supports SSL, for example WifiNINA.h WiFiSSLClient or WiFiBearSSLClient
     *               The caller must connect the Wifi client before invoking SQL statements:
     * @example
     * ```cpp
     * while (status != WL_CONNECTED) {
     *   Serial.print("Attempting to connect to SSID: ");
     *   Serial.println(ssid);
     *   // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
     *  status = WiFi.begin(ssid, pass);
     * 
     *   // wait 10 seconds for connection:
     *   delay(10000);
     * }
     * ```
     * @param neonPostgresConnectionString A connection string that connects to the Neon database, for example
     *                                     "postgresql://neondb_owner:password@ep-hungry-bird-4242.eu-central-1.aws.neon.tech/neondb?sslmode=require".
     *                                     This pointer/buffer must be valid for the complete lifetime of the ArduinoPostgresOverHttpProxyClient, it is NOT 
     *                                     copied
     * @param neonProxy The hostname of the neon proxy. Usually it can be easily derived from the connection string by extracting the hostname
     *                  as "api.<hostname>" for example "api.eu-central-1.aws.neon.tech".
     *                  This pointer/buffer must be valid for the complete lifetime of the ArduinoPostgresOverHttpProxyClient, it is NOT 
     *                  copied
     * @param proxyPort Proxy listening port. Usually 443 for https.
     */
  ArduinoPostgresOverHttpProxyClient(WiFiClient& client, const char* neonPostgresConnectionString, const char* neonProxy, int proxyPort = 443)
    : client(client), connstr(neonPostgresConnectionString), proxy(neonProxy), proxyPort(proxyPort) {
    request.clear();
    response.clear();
    request["params"].to<JsonArray>();
  }

  /**
     * @brief Specify the sql statement you want to execute. 
     *        The statement text can use parameter markers.
     *        If you use parameter markers you must also supply the parameter values before executing the
     *        statement.
     * @example
     * ```cpp
     * client.setQuery("INSERT INTO T1 (C1) VALUES ($1::int)");
     * JsonArray params = client.getParams();
     * while (moreWorkToDo) {
     *    params.clear();
     *    params.add(42);
     *    // execute statement and handle result
     *    ...
     * }
     * ```
     * 
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  void setQuery(const char* query) {
    request["query"] = query;
  }

  /**
     * @brief Get the parameter array to clear and set the parameter values for the next statement 
     *        execution.
     *      
     * @example
     * ```cpp
     * JsonArray params = client.getParams();
     * while (moreWorkToDo) {
     *    params.clear();
     *    params.add(42);
     *    // execute statement and handle result
     *    ...
     * }
     * ```
     * 
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  JsonArray getParams() {
    return request["params"].to<JsonArray>();
  }

  /**
    * Connect to proxy, send the SQL statement set with setQuery() and parse the result.
    * 
    * @param timeout maximum time in milliseconds to wait for response
    *
    * @return nullptr on success, error message in case of failure
    */
  const char* execute(unsigned long timeout = 20000) {
    if (client.connect(proxy, proxyPort)) {
      client.println("POST /sql HTTP/1.1");
      client.print("Host: ");
      client.println(proxy);
      client.print("Neon-Connection-String: ");
      client.println(connstr);
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      size_t length = measureJson(request);
      client.print(length);
      // Send payload after second new-line
      client.print("\r\n\r\n");  // double new line between headers and payload

      size_t written = serializeJson(request, client);
      if (written != length) {
        // Serial.print("Writing ");
        // Serial.print(length);
        // Serial.println(" chars:\n");
        // serializeJson(request, Serial);
        // Serial.print("\nserializeJson written: ");
        // Serial.println(written);
        client.stop();
        return "payload serialization error";
      }
      client.flush();
      // waiting for response
      unsigned long ms = millis();
      while (!client.available() && millis() - ms < timeout) {
        delay(0);
      }
      if (client.available()) {
        // Check HTTP status
        size_t bytes_read = client.readBytesUntil('\r', status, sizeof(status) - 1);
        status[bytes_read] = 0;
        // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK" or HTTP/1.1 400 Bad Request
        int status_code;
        sscanf(status + 9, "%3d", &status_code);
        if (status_code < 200 || (status_code >= 300 && status_code < 400) || status_code > 400) {
          client.stop();
          return status;
        }
        // Skip HTTP headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!client.find(endOfHeaders)) {
          client.stop();
          return "Invalid response";
        }

        DeserializationError err = deserializeJson(response, client);
        if (err) {
          client.stop();
          return err.c_str();
        }
        // else {
        //   Serial.println();
        //   serializeJson(response, Serial);
        //   Serial.println();
        // }
        client.stop();
        const char* errormessage = response["message"];
        if (errormessage != nullptr) {
          return errormessage;
        }
      } else {
        client.stop();
        return "query timed out";
      }
    } else {
      client.stop();
      return "cannot connect to proxy over Wifi";
    }
    return nullptr;
  }

  int getRowCount() {
    return response["rowCount"].as<int>();
  }

  JsonArray getRows() {
    return response["rows"].as<JsonArray>();
  }

  JsonArray getFields() {
    return response["fields"].as<JsonArray>();
  }

  /** Get complete query result as ArduinoJson JsonDocument.
    * Mostly used for debugging, normally
    * getRows(), getFields() and getRowCount() should be used.
    */
  JsonDocument& getRawJsonResult() {
    return response;
  }

  /** Print complete query result as ArduinoJson JsonDocument.
    * Mostly used for debugging.
    * @example
    * ```cpp
    * sqlClient.printRawJsonResult(Serial);
    * ```
    */
  void printRawJsonResult(Print& print) {
    print.println();
    serializeJson(response, print);
    print.println();
  }

private:
  JsonDocument request;
  JsonDocument response;
  WiFiClient& client;
  const char* connstr;
  const char* proxy;
  const int proxyPort;
  char status[32];
};

#endif /* ArduinoPostgresOverHttpProxyClient_H */