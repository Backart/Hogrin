#include "update_checker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

Update_Checker::Update_Checker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void Update_Checker::check_for_updates()
{
    QNetworkRequest request{QUrl(VERSION_URL)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Hogrin/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Update check failed:" << reply->errorString();
            emit check_failed(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);

        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "Update check: invalid JSON";
            emit check_failed("Invalid response");
            return;
        }

        QJsonObject obj      = doc.object();
        QString new_version  = obj["version"].toString();
        QString download_url = obj["url"].toString();
        QString changelog    = obj["changelog"].toString();

        if (new_version.isEmpty()) {
            emit check_failed("Missing version field");
            return;
        }

        if (compare_versions(new_version, CURRENT_VERSION) > 0) {
            qDebug() << "Update available:" << new_version;
            emit update_available(new_version, download_url, changelog);
        } else {
            qDebug() << "No update available, current:" << CURRENT_VERSION;
            emit no_update();
        }
    });
}

int Update_Checker::compare_versions(const QString &a, const QString &b)
{
    QStringList a_parts = a.split('.');
    QStringList b_parts = b.split('.');

    int len = qMax(a_parts.size(), b_parts.size());
    for (int i = 0; i < len; ++i) {
        int av = i < a_parts.size() ? a_parts[i].toInt() : 0;
        int bv = i < b_parts.size() ? b_parts[i].toInt() : 0;
        if (av != bv) return av - bv;
    }
    return 0;
}