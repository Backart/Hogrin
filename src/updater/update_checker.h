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

    static constexpr const char* CURRENT_VERSION = "1.0.0";
    static constexpr const char* VERSION_URL =
        "https://monk-hub.space/public.php/dav/files/QGB6xkr3zKF6FYd/version.json";

signals:
    void update_available(const QString &new_version,
                          const QString &download_url,
                          const QString &changelog);
    void no_update();
    void check_failed(const QString &reason);

private:
    QNetworkAccessManager *m_nam;

    static int compare_versions(const QString &a, const QString &b);
};

#endif // UPDATE_CHECKER_H