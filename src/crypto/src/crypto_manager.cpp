#include "crypto_manager.h"
#include <QDebug>
#include <stdexcept>

Crypto_Manager::Crypto_Manager()
{
    if (sodium_init() < 0)
        throw std::runtime_error("libsodium init failed");

    crypto_kx_keypair(m_public_key, m_secret_key);
    qDebug() << "Crypto_Manager: keypair generated";
}

QByteArray Crypto_Manager::public_key() const
{
    return QByteArray(reinterpret_cast<const char*>(m_public_key),
                      crypto_kx_PUBLICKEYBYTES);
}

bool Crypto_Manager::compute_shared_secret(const QByteArray &peer_public_key)
{
    if (m_secret_computed) {
        qDebug() << "Crypto_Manager: secret already computed, skipping";
        return true;
    }

    const unsigned char *peer_pk =
        reinterpret_cast<const unsigned char*>(peer_public_key.constData());

    int result = -1;

    if (public_key() > peer_public_key) {
        qDebug() << "Crypto_Manager: Keys compared. I am SERVER for this chat.";
        result = crypto_kx_server_session_keys(m_rx, m_tx,
                                               m_public_key,
                                               m_secret_key,
                                               peer_pk);
    } else {
        qDebug() << "Crypto_Manager: Keys compared. I am CLIENT for this chat.";
        result = crypto_kx_client_session_keys(m_rx, m_tx,
                                               m_public_key,
                                               m_secret_key,
                                               peer_pk);
    }

    if (result != 0) {
        qWarning() << "Crypto_Manager: key exchange failed";
        return false;
    }

    m_secret_computed = true;
    qDebug() << "Crypto_Manager: shared secret computed successfully";
    return true;
}

QByteArray Crypto_Manager::encrypt(const QByteArray &plaintext) const
{
    if (!m_secret_computed) {
        qWarning() << "Crypto_Manager: shared secret not computed";
        return {};
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    QByteArray ciphertext(plaintext.size() + crypto_secretbox_MACBYTES, '\0');

    crypto_secretbox_easy(
        reinterpret_cast<unsigned char*>(ciphertext.data()),
        reinterpret_cast<const unsigned char*>(plaintext.constData()),
        plaintext.size(),
        nonce,
        m_tx
        );

    // return: nonce + ciphertext
    return QByteArray(reinterpret_cast<const char*>(nonce),
                      crypto_secretbox_NONCEBYTES) + ciphertext;
}

QByteArray Crypto_Manager::decrypt(const QByteArray &ciphertext) const
{
    if (!m_secret_computed) {
        qWarning() << "Crypto_Manager: shared secret not computed";
        return {};
    }

    const int min_size = crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES;
    if (ciphertext.size() < min_size) {
        qWarning() << "Crypto_Manager: ciphertext too short";
        return {};
    }

    // interp: nonce | crypt data
    const unsigned char *nonce =
        reinterpret_cast<const unsigned char*>(ciphertext.constData());

    const unsigned char *encrypted =
        reinterpret_cast<const unsigned char*>(ciphertext.constData())
        + crypto_secretbox_NONCEBYTES;

    int encrypted_len = ciphertext.size() - crypto_secretbox_NONCEBYTES;

    QByteArray plaintext(encrypted_len - crypto_secretbox_MACBYTES, '\0');

    if (crypto_secretbox_open_easy(
            reinterpret_cast<unsigned char*>(plaintext.data()),
            encrypted,
            encrypted_len,
            nonce,
            m_rx) != 0)
    {
        qWarning() << "Crypto_Manager: decryption failed (tampered data?)";
        return {};
    }

    return plaintext;
}

// ── pass ───────────────────────────────────────────────────

QString Crypto_Manager::hash_password(const QString &password)
{
    char hash[crypto_pwhash_STRBYTES];

    const QByteArray pw = password.toUtf8();

    if (crypto_pwhash_str(
            hash,
            pw.constData(),
            pw.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        qWarning() << "Crypto_Manager: password hashing failed (out of memory?)";
        return {};
    }

    return QString::fromUtf8(hash);
}

bool Crypto_Manager::verify_password(const QString &password, const QString &hash)
{
    const QByteArray pw   = password.toUtf8();
    const QByteArray hash_bytes = hash.toUtf8();
    return crypto_pwhash_str_verify(
               hash_bytes.constData(),
               pw.constData(),
               pw.size()) == 0;
}

void Crypto_Manager::set_identity(const Crypto_Manager &source)
{
    memcpy(m_public_key, source.m_public_key, crypto_kx_PUBLICKEYBYTES);
    memcpy(m_secret_key, source.m_secret_key, crypto_kx_SECRETKEYBYTES);
    m_secret_computed = false; // сессионные ключи нужно пересчитать
}

QByteArray Crypto_Manager::secret_key() const
{
    return QByteArray(reinterpret_cast<const char*>(m_secret_key),
                      crypto_kx_SECRETKEYBYTES);
}

void Crypto_Manager::load_keypair(const QByteArray &pub, const QByteArray &sec)
{
    if (pub.size() == crypto_kx_PUBLICKEYBYTES && sec.size() == crypto_kx_SECRETKEYBYTES) {
        memcpy(m_public_key, pub.constData(), crypto_kx_PUBLICKEYBYTES);
        memcpy(m_secret_key, sec.constData(), crypto_kx_SECRETKEYBYTES);
        qDebug() << "Crypto_Manager: identity keypair loaded from DB";
    } else {
        qWarning() << "Crypto_Manager: failed to load keypair, invalid sizes!";
    }
}
