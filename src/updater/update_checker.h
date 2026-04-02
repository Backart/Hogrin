#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class Update_Checker : public QObject
{
    Q_OBJECT

public:
    explicit Update_Checker(QObject *parent = nullptr);

    Q_INVOKABLE void check_for_updates();
    Q_INVOKABLE void download_and_install(const QString &url);
    Q_INVOKABLE void trigger_install();

    static constexpr const char* CURRENT_VERSION = "1.0.9";
    static constexpr const char* VERSION_URL =
        "https://monk-hub.space/public.php/dav/files/QGB6xkr3zKF6FYd/version.json";

signals:
    void update_available(const QString &new_version,
                          const QString &download_url,
                          const QString &changelog);
    void no_update();
    void check_failed(const QString &reason);
    void download_progress(int percent);
    void download_finished();
    void download_failed(const QString &reason);

private:
    QNetworkAccessManager *m_nam;
    static int compare_versions(const QString &a, const QString &b);
    void install_apk(const QString &apk_path);
    QString m_pending_apk_path;
};

#endif // UPDATE_CHECKER_H