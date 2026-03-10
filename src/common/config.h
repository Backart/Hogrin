#ifndef CONFIG_H
#define CONFIG_H
#include <QByteArray>

namespace Config {
// ВИДАЛИТИ:
// constexpr const char* DB_HOST      = "100.114.164.127";
// constexpr int         DB_PORT      = 5433;
// constexpr const char* DB_NAME      = "hogrin";
// constexpr const char* DB_USER      = "hogrin_app";
// constexpr const char* DB_PASSWORD  = "OuMtKpeqC3zt8o+IqQgK";

constexpr const char* BOOTSTRAP_SERVER = "2a02:8109:b224:e500:6e4b:90ff:feb9:8c7";
constexpr quint16     BOOTSTRAP_PORT   = 47832;
constexpr const char* HANDSHAKE_TOKEN  = "Y)qrLkv;SfuQ~!7";

constexpr int P2P_TIMEOUT_MS = 5000;
constexpr int RELAY_POLL_INTERVAL_MS = 5000;

}

#endif // CONFIG_H
