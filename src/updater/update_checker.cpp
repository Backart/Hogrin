#include "update_checker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <QtCore/private/qandroidextras_p.h>
#endif

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

void Update_Checker::download_and_install(const QString &url)
{
    qDebug() << "Downloading APK from:" << url;

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::downloadProgress,
            this, [this](qint64 received, qint64 total) {
                if (total > 0)
                    emit download_progress(static_cast<int>(received * 100 / total));
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Download failed:" << reply->errorString();
            emit download_failed(reply->errorString());
            return;
        }
        QString apk_path = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation) + "/hogrin_update.apk";

        QFile file(apk_path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit download_failed("Cannot write APK file");
            return;
        }
        file.write(reply->readAll());
        file.close();

        qDebug() << "APK saved to:" << apk_path;
        emit download_finished();
        install_apk(apk_path);
    });
}

void Update_Checker::install_apk(const QString &apk_path)
{
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        qWarning() << "Cannot get Android context";
        return;
    }

    QJniObject package_name = activity.callObjectMethod<jstring>("getPackageName");
    QString authority = package_name.toString() + ".qtprovider";

    QJniObject path = QJniObject::fromString(apk_path);
    QJniObject file("java/io/File", "(Ljava/lang/String;)V", path.object());

    QJniObject uri = QJniObject::callStaticObjectMethod(
        "androidx/core/content/FileProvider",
        "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
        activity.object(),
        QJniObject::fromString(authority).object(),
        file.object());

    if (!uri.isValid()) {
        qWarning() << "FileProvider URI is invalid";
        return;
    }

    QJniObject intent("android/content/Intent",
                      "(Ljava/lang/String;)V",
                      QJniObject::fromString("android.intent.action.VIEW").object());

    QJniObject mime = QJniObject::fromString("application/vnd.android.package-archive");
    intent.callObjectMethod("setDataAndType",
                            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object(), mime.object());

    jint flag_grant = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    jint flag_activity = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");

    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", flag_grant | flag_activity);

    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    qDebug() << "Install intent launched";
#else
    Q_UNUSED(apk_path)
    qDebug() << "install_apk: not on Android";
#endif
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