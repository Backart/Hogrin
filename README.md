# Hogrin

Decentralized anonymous messenger with end-to-end encryption, peer-to-peer connections, and local data storage.

Built with Qt6/QML and libsodium. Supports desktop (macOS, Linux) and Android.

## Features

- **End-to-end encryption** — XSalsa20-Poly1305 (libsodium) with per-message random nonces
- **P2P messaging** — direct TCP connections between peers with automatic NAT detection
- **Relay fallback** — messages routed through bootstrap server when P2P is unavailable (NAT/CGNAT)
- **Local storage** — all messages stored in SQLite on-device, never on the server
- **Session persistence** — automatic session restore on app restart
- **Dark/Light theme** — toggleable UI theme with responsive layout (phone/tablet/desktop)
- **OTA updates** — in-app update checker and installer (Android)

## Architecture

```
┌─────────────┐         ┌─────────────────┐         ┌─────────────┐
│   Client A  │◄──P2P──►│  Bootstrap      │◄──P2P──►│   Client B  │
│             │         │  Server         │         │             │
│ ┌─────────┐ │         │                 │         │ ┌─────────┐ │
│ │ SQLite  │ │         │ - Peer registry │         │ │ SQLite  │ │
│ │ (local) │ │         │ - Auth (Argon2) │         │ │ (local) │ │
│ └─────────┘ │         │ - Relay queue   │         │ └─────────┘ │
└─────────────┘         └─────────────────┘         └─────────────┘
```

Clients connect to a bootstrap server for user registration, peer discovery, and relay. When peers are reachable, they communicate directly via encrypted TCP. When behind NAT, messages are relayed through the bootstrap server — still end-to-end encrypted.

## Cryptography

| Purpose            | Algorithm                          |
|--------------------|------------------------------------|
| Key exchange       | Curve25519 (`crypto_kx_*`)         |
| Message encryption | XSalsa20-Poly1305 (`crypto_secretbox`) |
| Password hashing   | Argon2i (`crypto_pwhash`)          |

Each peer conversation uses unique session keys derived from Diffie-Hellman key exchange. Messages are authenticated and encrypted with a per-message random 24-byte nonce.

## Prerequisites

- **CMake** 3.16+
- **Qt** 6.5+ (Core, Gui, Qml, Quick, Network, Sql)
- **libsodium** (auto-fetched from source if not found)
- **C++17** compiler

### macOS

```bash
brew install cmake qt libsodium
```

### Ubuntu/Debian

```bash
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-tools-dev libsodium-dev
```

## Build & Run

```bash
git clone https://github.com/Backart/Hogrin.git
cd Hogrin

cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"   # macOS
# cmake -B build                                            # Linux (Qt6 in system paths)

cmake --build build
./build/appHogrin
```

### Android

The project includes Android support with:
- Min SDK 26 (Android 8.0), Target SDK 34 (Android 14)
- OpenSSL auto-fetched via [KDAB/android_openssl](https://github.com/KDAB/android_openssl)
- FileProvider configured for OTA APK installation

Build with Qt Creator or the Qt Android toolchain via CMake.

## Project Structure

```
src/
├── app/            # Application entry point
├── common/         # Shared types (MessageType, PeerInfo, DataPacket) and config
├── core/           # Messenger orchestrator — routing, relay logic, message handling
├── crypto/         # libsodium wrapper — key exchange, encrypt/decrypt, password hashing
├── db/             # SQLite storage — messages, outbox queue, identity keys, contacts
├── network/        # TCP server/client, connection framing, bootstrap protocol
├── ui/
│   ├── backend/    # C++/QML bridge (UIHandler)
│   └── qml/        # Qt Quick UI — Main, ChatArea, ChatList, AuthScreen, components
├── updater/        # OTA update checker and downloader
assets/
└── fonts/          # NotoEmoji-Regular.ttf
android/            # AndroidManifest, FileProvider config
```

## Protocol

### Bootstrap Server (TCP, line-based)

| Command | Description |
|---------|-------------|
| `AUTH_REGISTER:{nick}:{pass}` | Create account |
| `AUTH_LOGIN:{nick}:{pass}` | Login, returns session token |
| `AUTH_VERIFY:{token}` | Restore session |
| `REGISTER:{nick}:{port}:{pubkey}` | Register peer endpoint |
| `FIND:{nick}` | Discover peer IP/port/pubkey |
| `STORE:{nick}:{blob}` | Relay encrypted message |
| `FETCH:{nick}` | Retrieve queued relay messages |

### P2P (TCP, binary)

1. **Handshake**: `{nickname}:{pubkey_hex}` (plaintext)
2. **Messages**: `[4-byte size][encrypted payload]` — payload is a serialized `DataPacket`

## License

[MIT](LICENSE) — Copyright (c) 2025 Backart
