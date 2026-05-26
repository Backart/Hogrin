# Security and Logic Analysis of the Hogrin Messenger Codebase

After an extensive review of the provided code, several security vulnerabilities, architectural concerns, and logical bugs have been identified. Below is a detailed report of the findings and suggested remedies.

---

### 1. Critical Security Vulnerability: Blind Trust in Relay Message Public Keys (MITM & Spoofing)
* **Location:** `Messenger_Core::messages_fetched` in `src/core/src/messenger_core.cpp`
* **Severity:** **Critical**
* **Description:** 
  When the client polls the bootstrap server and fetches relay messages, the incoming packet contains a header formatted as `sender_nick|sender_pubkey|encrypted_payload`. 
  If the receiver doesn't have an established session (`!crypto->is_ready()`) with that contact yet, the code blindly takes `peer_pubkey` from the header and computes session keys:
  ```cpp
  if (!crypto->is_ready()) {
      crypto->set_identity(*m_identity_crypto);
      crypto->compute_shared_secret(peer_pubkey);
  }
  ```
  An attacker can easily store a message on the bootstrap server under a victim's nickname, but insert the **attacker's own public key** in the header. The receiver's client will blindly use the attacker's public key, establish shared session keys with the attacker, decrypt the message, and display it as a legitimate message from the victim.
* **Remedy:** 
  Never trust public keys sent in the message header of unauthenticated relay messages. Instead, the public key should be cross-referenced with the trusted local contact key database using `m_local_db->get_contact_key(sender_nick)`. If the key is missing or differs, the message must be dropped or flagged, and a lookup request (`find_peer`) must be sent to the bootstrap server to obtain the authoritative public key.

---

### 2. Security Concern: Sensitive Cryptographic Keys Left in Memory
* **Location:** `Crypto_Manager` class (`src/crypto/include/crypto_manager.h` and `src/crypto/src/crypto_manager.cpp`)
* **Severity:** **Medium**
* **Description:** 
  `Crypto_Manager` handles highly sensitive key materials, including:
  * Identity secret key (`m_secret_key`)
  * RX and TX session keys (`m_rx`, `m_tx`)
  
  These arrays are stored as raw buffers in class instances. When a connection is dropped or a user logs out, the `Crypto_Manager` instances are deleted, but the underlying heap/stack memory is not zeroed out. This makes the private keys susceptible to recovery via memory-forensics tools, core dumps, or other application memory inspection exploits.
* **Remedy:** 
  Implement a destructor in `Crypto_Manager` that clears sensitive buffers using libsodium's secure zeroing function:
  ```cpp
  Crypto_Manager::~Crypto_Manager() {
      sodium_memzero(m_secret_key, sizeof(m_secret_key));
      sodium_memzero(m_rx, sizeof(m_rx));
      sodium_memzero(m_tx, sizeof(m_tx));
  }
  ```

---

### 3. Logical Bug: Inefficient Relay Message Polling (Sluggish Inbox Sync)
* **Location:** `Messenger_Core::poll_relay_messages` and `Bootstrap_Client::fetch_messages`
* **Severity:** **Low-Medium**
* **Description:** 
  The app polls for relay messages once every 5 seconds (`Config::RELAY_POLL_INTERVAL_MS`). The bootstrap server returns exactly one message per `FETCH` request (or `EMPTY`).
  If a user has been offline and has 20 queued messages, it will take **100 seconds (almost 2 minutes)** to download and display all messages because the client only requests one message per 5-second interval.
* **Remedy:** 
  When receiving a message from `messages_fetched`, check if a message was successfully processed. If so, immediately trigger another `fetch_messages()` call instead of waiting for the next timer tick. Only allow the polling timer to rest when an `EMPTY` response is received.

---

### 4. Database Warning & Resource Leak: Incorrect QSqlDatabase Connection Teardown
* **Location:** `Local_DB::init` in `src/db/src/local_db.cpp`
* **Severity:** **Low**
* **Description:** 
  When re-initializing the database (e.g. during login or session switching), the code closes and attempts to remove the connection:
  ```cpp
  if (m_db.isOpen()) {
      m_db.close();
      QSqlDatabase::removeDatabase(m_connection_name);
  }
  ```
  Since `m_db` is a member variable holding an active connection instance, calling `removeDatabase` while `m_db` still exists inside the same scope results in Qt outputting console warnings: `connection ... is still in use, all queries will cease to work`.
* **Remedy:** 
  To safely remove a connection, all instances of `QSqlDatabase` and `QSqlQuery` referring to it must go out of scope before calling `removeDatabase`. Assign an empty connection object first to release the handle:
  ```cpp
  m_db = QSqlDatabase(); 
  QSqlDatabase::removeDatabase(m_connection_name);
  ```

---

### 5. API/Naming Discrepancy: `DB_Manager::validateUser` Parameter Naming
* **Location:** `DB_Manager::validateUser` in `src/db/src/db_manager.cpp`
* **Severity:** **Low**
* **Description:** 
  The method signature defines the second argument as `passwordHash`:
  ```cpp
  bool DB_Manager::validateUser(const QString &nickname, const QString &passwordHash)
  ```
  However, inside the implementation, this parameter is directly passed to `crypto_pwhash_str_verify` as if it were the **cleartext password**:
  ```cpp
  return crypto_pwhash_str_verify(
             stored_hash.toUtf8().constData(),
             passwordHash.toUtf8().constData(),
             passwordHash.toUtf8().size()) == 0;
  ```
  If the bootstrap server sends a pre-hashed password over the network, this check will fail (since it tries to verify a hash of a hash). If it is indeed a cleartext password, the parameter name is extremely misleading to developers and can easily lead to integration bugs.
* **Remedy:** 
  Rename the argument to `password` or `plainPassword` to ensure clarity and prevent future developers from passing pre-hashed data to this verification routine.
