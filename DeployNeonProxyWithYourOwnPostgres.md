# Deploying the Neon Proxy for your self-hosted PostgreSQL server

Neon proxy is the middle-man between this Arduino library (database client) and your PostgreSQL server. It is a lightweight, open-source proxy that allows you to connect to your PostgreSQL database securely and efficiently.

Normally PostgreSQL servers only accept TCP connections from clients like psql or libraries like libpq that speak the Postgres Wire Protocol.
I am not aware of any library implementation for microcontrollers that implements the Postgres Wire Protocol client.

Thus we use a Neon proxy as a lightweight HTTP proxy that accepts HTTPS requests and translates them into Postgres Wire Protocol requests.
Thus on the Arduino/Microcontroller we "only" need a TLS client library and JSON generator/parser library to communicate with the proxy.


```txt
[Microcontroller with TLS client library] 

            ^
            |
JSON payload with SQL or query results
            |
            v

      [Neon proxy] 

            ^
            |
     wire protocol payload
            |
            v

    [PostgreSQL server]
```

The simplest way to deploy the Neon proxy is to use the Neon database as a server on [neon.tech](https://neon.tech/) which comes with the Neon proxy pre-installed. If you use that choice you can skip this document.

This document describes how to deploy the Neon proxy on your own PostgreSQL server.

## Prerequisites

You run your own PostgreSQL server with a DNS domain name assigned to the server's IP address.
You have a PostgreSQL server running on the server.
You have a TLS certificate for the domain name assigned to the server or can generate one.
(Below we describe a setup with self-signed TLS certificates, but this is not recommended for production use.)

You can install development tools like `git`, openssl and Rust on the server.
You hae permission to open TCP port 4444 (or another port that your define for the proxy) on the server.

## Preparing your PostgreSQL server
You need to create a PostgreSQL user and database for the Neon proxy.

On your server verify that you can connect to the PostgreSQL server using the `psql` command line tool.

```bash
sudo su - postgres

psql
```

Then, in your PostgreSQL shell, set a confidential password for the `postgres` user (or any other user you want to use for the Neon proxy) and prepare some metadata tables used by the Neon proxy.

```sql
-- in psql shell
ALTER USER postgres WITH PASSWORD 'proxy_password';
CREATE SCHEMA IF NOT EXISTS neon_control_plane;
CREATE TABLE neon_control_plane.endpoints (endpoint_id VARCHAR(255) PRIMARY KEY, allowed_ips VARCHAR(255));
```

Then create a new user that your microcontroller can use to connect to the Neon proxy.

```sql
CREATE ROLE arduino WITH SUPERUSER LOGIN PASSWORD 'arduino_password';
```

## Set up Neon proxy

### Definition of variables you need to define:

```bash
# listening port of the neon proxy (receiving SQL over HTTPS reqeusts)
# this port must be open in your postgresql server firewall to receive inbound TCP requests
export PROXY_PORT=4444
# user and password for the neon proxy to connect to the PostgreSQL server for control plane purposes
export PROXY_USER=postgres
export PROXY_PASSWORD=proxy_password
# end user and password for the microcontroller to connect through the Neon proxy to the PostgreSQL server
export ARDUINO_USER=arduino
export ARDUINO_PASSWORD=arduino_password
# the host / domain name of your server
export POSTGRES_HOSTNAME=yourserver.yourdomain.com
```

### Clone the Neon proxy repository

```bash
# Clone the Neon proxy repository
git clone --depth 1 https://github.com/neondatabase/neon.git
cd neon
```

### prepare the TLS certificate for your server

If you own the domain name `yourserver.yourdomain.com` you can use the free TLS certificate service from [Let's Encrypt](https://letsencrypt.org/).
You can use the `certbot` tool to create a TLS certificate for your domain name.

In this document we describe a test setup with self-signed TLS certificates created with the `openssl` command line tool.

```bash
# use your own domain name
export POSTGRES_HOSTNAME=yourserver.yourdomain.com
# then create a self-signed TLS certificate for your domain name
# install openssl, then
openssl req -new -x509 -days 365 -nodes -text -out server.crt -keyout server.key -subj "/CN=*.${POSTGRES_HOSTNAME}"
```

### Building and running the Neon proxy

```bash
# Install Rust and Cargo, see https://www.rust-lang.org/tools/install, then

# Compile and run the Neon proxy
# we need to use --features testing to enable using the local PostgreSQL server as authentication backend
RUST_LOG=proxy LOGFMT=text cargo run -p proxy --bin proxy --features testing -- --auth-backend postgres --auth-endpoint "postgresql://${PROXY_USER}:${PROXY_PASSWORD}@127.0.0.1:5432/postgres" -c server.crt -k server.key --wss 0.0.0.0:${PROXY_PORT} 
```

### Configure your arduino_secrets.h file

In your Arduino IDE when preparing the sketch for your microcontroller you need to set the connection parameters for the Neon proxy and the PostgreSQL server:

copy [examples/BasicExample_ESP32C6_with_upstream_postgres/arduino_secrets.h.example](examples/BasicExample_ESP32C6_with_upstream_postgres/arduino_secrets.h.example) to `arduino_secrets.h` and edit the file to set the following variables:

```cpp
#define SECRET_SSID "yourWifiSSID"  // your network SSID (name)
#define SECRET_PASS "yourWifiPassword"  // your network password (use for WPA, or use as key for WEP)
#define NEON_PROXY "your self-deployed Neon proxy's hostname" // e.g. "yourserver.yourdomain.com", see ${POSTGRES_HOSTNAME} above
#define DATABASE_URL "your postgresql connection string" // replace all variable in the following string by your actual values "postgresql://${ARDUINO_USER}:${ARDUINO_PASSWORD}@${POSTGRES_HOSTNAME}:${PROXY_PORT}/postgres"
```

### Modify your Arduino sketch to initialize the client with the correct hostname, database_url and port

```cpp
WiFiClientSecure client;
// use ${PROXY_PORT} above for the port number
NeonPostgresOverHTTPProxyClient sqlClient(client, DATABASE_URL, NEON_PROXY, 4444);
```

### Security notice - not recommended for production use

To verify the Neon proxy server's TLS certificate the Arduino side client library needs to have the CA certificate of the TLS certificate authority that signed the server's TLS certificate.

If you have an offical TLS certificate from a trusted certificate authority in most cases the trust chain already comes pre-installed in the Arduino core libraries.
If you use a self-signed TLS certificate you need to either install the CA certificate of the self-signed TLS certificate authority in the Arduino core libraries (not described here) or you need to use the `setInsecure()` method of the `WiFiClientSecure` class to disable TLS certificate verification.

```cpp
client.setInsecure(); // disables TLS certificate validation, enabling man-in-the-middle attacks
```