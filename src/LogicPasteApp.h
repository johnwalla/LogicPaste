#ifndef LOGICPASTEAPP_H
#define LOGICPASTEAPP_H

#include <bb/cascades/Application>
#include <QtNetwork/QNetworkAccessManager>

#include "Pastebin.h"

using namespace bb::cascades;

// Forward declarations
namespace bb { namespace cascades {
class NavigationPane;
class Page;
} }


class LogicPasteApp : public QObject {
    Q_OBJECT

public:
    LogicPasteApp();

    Q_INVOKABLE QString getSettingValue(const QString &keyName);

public slots:
    void onRequestLogin();
    void onRequestLogout();
    void onCreateAccount();

    void onProcessLogin(QString username, QString password);

private slots:
    void onLoginComplete();
    void onLoginFailed(QString message);

signals:
    void loginFailed(QString message);
    void settingsUpdated();

private:
    NavigationPane *navigationPane_;
    Page *pastePage_;
    Page *historyPage_;
    Page *settingsPage_;

    Pastebin pastebin_;
};

#endif // LOGICPASTEAPP_H
