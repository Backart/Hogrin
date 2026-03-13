#ifndef CONFIG_H
#define CONFIG_H
#include <QByteArray>

namespace Config {

constexpr const char* BOOTSTRAP_SERVER = "bootstrap.monk-hub.space";
constexpr quint16     BOOTSTRAP_PORT   = 47832;

constexpr int P2P_TIMEOUT_MS = 5000;
constexpr int RELAY_POLL_INTERVAL_MS = 5000;

}

#endif // CONFIG_H
