#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#pragma once

#include <QByteArray>
#include <QString>
#include <sodium.h>

class Crypto_Manager
{
public:
    Crypto_Manager();

    QByteArray public_key() const;
    QByteArray secret_key() const;

    void load_keypair(const QByteArray &pub, const QByteArray &sec);

    bool compute_shared_secret(const QByteArray &peer_public_key);

    QByteArray encrypt(const QByteArray &plaintext) const;
    QByteArray decrypt(const QByteArray &ciphertext) const;

    // hash salz
    static QString hash_password(const QString &password);
    static bool    verify_password(const QString &password, const QString &hash);

    bool is_ready() const { return m_secret_computed; }

    void set_identity(const Crypto_Manager &source);

private:
    // Keypair
    unsigned char m_public_key[crypto_kx_PUBLICKEYBYTES];
    unsigned char m_secret_key[crypto_kx_SECRETKEYBYTES];

    // Shared session keys (rx — получение, tx — отправка)
    unsigned char m_rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char m_tx[crypto_kx_SESSIONKEYBYTES];

    bool m_secret_computed = false;
};

#endif // CRYPTO_MANAGER_H
