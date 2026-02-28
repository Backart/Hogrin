#ifndef CONFIG_H
#define CONFIG_H

#include <QByteArray>

namespace Config {
constexpr const char* DB_HOST     = "100.114.164.127";
constexpr int         DB_PORT     = 5433;
constexpr const char* DB_NAME     = "hogrin";
constexpr const char* DB_USER     = "hogrin_app";
constexpr const char* DB_PASSWORD = "OuMtKpeqC3zt8o+IqQgK";

constexpr const char* BOOTSTRAP_SERVER = "YOUR_SERVER_IP";
constexpr quint16     BOOTSTRAP_PORT   = 47832;
constexpr const char* HANDSHAKE_TOKEN  = "YOUR_TOKEN";
}

#endif // CONFIG_H
