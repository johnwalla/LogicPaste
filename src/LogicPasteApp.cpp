#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/NavigationPane>
#include <bb/cascades/Page>
#include <bb/cascades/Label>
#include <bb/cascades/TextField>
#include <bb/cascades/TextArea>
#include <bb/cascades/ActionItem>
#include <bb/cascades/ListView>
#include <bb/cascades/DropDown>
#include <bb/cascades/WebView>

#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>

#include <bps/navigator.h>

#include "LogicPasteApp.h"

#include "config.h"

LogicPasteApp::LogicPasteApp() {
    QCoreApplication::setOrganizationName("LogicProbe");
    QCoreApplication::setApplicationName("LogicPaste");

    pasteModel_ = new PasteModel(this);

    QmlDocument *qml = QmlDocument::create(this, "main.qml");
    qml->setContextProperty("cs", this);
    qml->setContextProperty("model", pasteModel_);

    if(!qml->hasErrors()) {
        navigationPane_ = qml->createRootNode<NavigationPane>();
        if(navigationPane_) {
            pastePage_ = navigationPane_->findChild<Page*>("pastePage");
            connect(pastePage_, SIGNAL(submitPaste()), this, SLOT(onSubmitPaste()));

            historyPage_ = navigationPane_->findChild<Page*>("historyPage");
            historyPage_->findChild<ActionItem*>("refreshAction")->setEnabled(pasteModel_->isAuthenticated());
            connect(historyPage_, SIGNAL(refreshPage()), pasteModel_, SLOT(refreshHistory()));
            connect(historyPage_, SIGNAL(openPaste(QString)), this, SLOT(onOpenPaste(QString)));
            historyPage_->findChild<ListView*>("pasteList")->setDataModel(pasteModel_->historyModel());
            connect(pasteModel_, SIGNAL(historyUpdating()), historyPage_, SLOT(onRefreshStarted()));
            connect(pasteModel_, SIGNAL(historyUpdated()), historyPage_, SLOT(onRefreshComplete()));

            trendingPage_ = navigationPane_->findChild<Page*>("trendingPage");
            trendingPage_->findChild<ActionItem*>("refreshAction")->setEnabled(true);
            connect(trendingPage_, SIGNAL(refreshPage()), pasteModel_, SLOT(refreshTrending()));
            connect(trendingPage_, SIGNAL(openPaste(QString)), this, SLOT(onOpenPaste(QString)));
            trendingPage_->findChild<ListView*>("pasteList")->setDataModel(pasteModel_->trendingModel());
            connect(pasteModel_, SIGNAL(trendingUpdating()), trendingPage_, SLOT(onRefreshStarted()));
            connect(pasteModel_, SIGNAL(trendingUpdated()), trendingPage_, SLOT(onRefreshComplete()));

            settingsPage_ = navigationPane_->findChild<Page*>("settingsPage");
            connect(settingsPage_, SIGNAL(requestLogin()), this, SLOT(onRequestLogin()));
            connect(settingsPage_, SIGNAL(requestLogout()), this, SLOT(onRequestLogout()));
            connect(settingsPage_, SIGNAL(refreshUserDetails()), pasteModel_, SLOT(refreshUserDetails()));
            connect(pasteModel_, SIGNAL(userDetailsUpdated(PasteUser)), this, SLOT(onUserDetailsUpdated(PasteUser)));

            Application::setScene(navigationPane_);

            if(pasteModel_->isAuthenticated()) {
                onUserDetailsUpdated(pasteModel_->pasteUserDetails());
            }
        }
    }
}

void LogicPasteApp::onRequestLogin() {
    qDebug() << "onRequestLogin()";

    QmlDocument *qml = QmlDocument::create(this, "LoginPage.qml");
    if(!qml->hasErrors()) {
        Page *loginPage = qml->createRootNode<Page>();
        qml->setContextProperty("cs", this);
        connect(loginPage, SIGNAL(processLogin(QString,QString)), this, SLOT(onProcessLogin(QString,QString)));
        connect(loginPage, SIGNAL(createAccount()), this, SLOT(onCreateAccount()));
        navigationPane_->push(loginPage);
    }
}

void LogicPasteApp::onProcessLogin(QString username, QString password) {
    qDebug() << "onProcessLogin" << username << "," << password;
    connect(pasteModel_->pastebin(), SIGNAL(loginComplete(QString)), this, SLOT(onLoginComplete(QString)));
    connect(pasteModel_->pastebin(), SIGNAL(loginFailed(QString)), this, SLOT(onLoginFailed(QString)));
    pasteModel_->pastebin()->login(username, password);
}

void LogicPasteApp::onLoginComplete(QString apiKey) {
    disconnect(pasteModel_->pastebin(), SIGNAL(loginComplete()), this, SLOT(onLoginComplete()));
    disconnect(pasteModel_->pastebin(), SIGNAL(loginFailed(QString)), this, SLOT(onLoginFailed(QString)));

    Label *label = settingsPage_->findChild<Label*>("keyLabel");
    label->setText(QString("API Key: %1").arg(apiKey));
    label->setVisible(true);

    emit settingsUpdated();
    historyPage_->findChild<ActionItem*>("refreshAction")->setEnabled(pasteModel_->isAuthenticated());
    navigationPane_->popAndDelete();
}

void LogicPasteApp::onLoginFailed(QString message) {
    disconnect(pasteModel_->pastebin(), SIGNAL(loginComplete()), this, SLOT(onLoginComplete()));
    disconnect(pasteModel_->pastebin(), SIGNAL(loginFailed(QString)), this, SLOT(onLoginFailed(QString)));

    emit loginFailed(message);
}

void LogicPasteApp::onUserDetailsUpdated(PasteUser pasteUser) {
    qDebug() << "onUserDetailsUpdated()";

    Label *label;

    if(!pasteUser.username().isEmpty()) {
        label = settingsPage_->findChild<Label*>("userLabel");
        label->setText(QString("Username: %1").arg(pasteUser.username()));
        label->setVisible(true);
    }

    if(pasteModel_->isAuthenticated()) {
        label = settingsPage_->findChild<Label*>("keyLabel");
        label->setText(QString("API Key: %1").arg(pasteUser.apiKey()));
        label->setVisible(true);
    }

    if(!pasteUser.website().isEmpty()) {
        label = settingsPage_->findChild<Label*>("websiteLabel");
        label->setText(QString("Website: %1").arg(pasteUser.website()));
        label->setVisible(true);
    }

    if(!pasteUser.email().isEmpty()) {
        label = settingsPage_->findChild<Label*>("emailLabel");
        label->setText(QString("Email: %1").arg(pasteUser.email()));
        label->setVisible(true);
    }

    if(!pasteUser.location().isEmpty()) {
        label = settingsPage_->findChild<Label*>("locationLabel");
        label->setText(QString("Location: %1").arg(pasteUser.location()));
        label->setVisible(true);
    }

    DropDown *dropDown;

    QSettings settings;
    if(!pasteUser.pasteFormatShort().isEmpty()) {
        settings.setValue("user_format_short", pasteUser.pasteFormatShort());
        dropDown = settingsPage_->findChild<DropDown*>("formatDropDown");
        for(int i = dropDown->optionCount() - 1; i >= 0; --i) {
            if(dropDown->at(i)->value() == pasteUser.pasteExpiration()) {
                dropDown->setSelectedIndex(i);
                break;
            }
        }
    }
    if(!pasteUser.pasteExpiration().isEmpty()) {
        settings.setValue("user_expiration", pasteUser.pasteExpiration());
        dropDown = settingsPage_->findChild<DropDown*>("expirationDropDown");
        for(int i = dropDown->optionCount() - 1; i >= 0; --i) {
            if(dropDown->at(i)->value() == pasteUser.pasteExpiration()) {
                dropDown->setSelectedIndex(i);
                break;
            }
        }
    }

    int visibilityValue = static_cast<int>(pasteUser.pasteVisibility());
    settings.setValue("user_private", visibilityValue);
    dropDown = settingsPage_->findChild<DropDown*>("exposureDropDown");
    dropDown->setSelectedIndex(visibilityValue);

    QMetaObject::invokeMethod(settingsPage_, "userDetailsRefreshed");
}

void LogicPasteApp::onCreateAccount() {
    qDebug() << "onCreateAccount()";

    navigator_invoke("http://pastebin.com/signup", 0);
}

void LogicPasteApp::onRequestLogout() {
    qDebug() << "onRequestLogout()";

    pasteModel_->pastebin()->logout();

    emit settingsUpdated();
    historyPage_->findChild<ActionItem*>("refreshAction")->setEnabled(pasteModel_->isAuthenticated());
}

void LogicPasteApp::onSubmitPaste() {
    qDebug() << "onSubmitPaste()";

    QString pasteContent = pastePage_->findChild<TextArea*>("pasteTextField")->text();
    QString pasteTitle = pastePage_->findChild<TextField*>("pasteTitleField")->text();

    DropDown *expirationDropDown = pastePage_->findChild<DropDown*>("expirationDropDown");
    QString expiration = expirationDropDown->at(expirationDropDown->selectedIndex())->value().toString();

    DropDown *formatDropDown = pastePage_->findChild<DropDown*>("formatDropDown");
    QString format = formatDropDown->at(formatDropDown->selectedIndex())->value().toString();

    DropDown *exposureDropDown = pastePage_->findChild<DropDown*>("exposureDropDown");
    int value = exposureDropDown->at(exposureDropDown->selectedIndex())->value().toInt();
    PasteListing::Visibility visibility;
    if(value == 0) {
        visibility = PasteListing::Public;
    } else if(value == 1) {
        visibility = PasteListing::Unlisted;
    } else {
        visibility = PasteListing::Private;
    }

    connect(pasteModel_->pastebin(), SIGNAL(pasteComplete(QString)), this, SLOT(onPasteComplete(QString)));
    connect(pasteModel_->pastebin(), SIGNAL(pasteFailed(QString)), this, SLOT(onPasteFailed(QString)));

    pasteModel_->pastebin()->submitPaste(pasteContent, pasteTitle, format, expiration, visibility);
}

void LogicPasteApp::onPasteComplete(QString pasteUrl) {
    Q_UNUSED(pasteUrl)
    qDebug() << "onPasteComplete()";
    disconnect(pasteModel_->pastebin(), SIGNAL(pasteComplete(QString)), this, SLOT(onPasteComplete(QString)));
    disconnect(pasteModel_->pastebin(), SIGNAL(pasteFailed(QString)), this, SLOT(onPasteFailed(QString)));

    QMetaObject::invokeMethod(pastePage_, "pasteSuccess");
}

void LogicPasteApp::onPasteFailed(QString message) {
    Q_UNUSED(message)
    qDebug() << "onPasteFailed()";
    disconnect(pasteModel_->pastebin(), SIGNAL(pasteComplete(QString)), this, SLOT(onPasteComplete(QString)));
    disconnect(pasteModel_->pastebin(), SIGNAL(pasteFailed(QString)), this, SLOT(onPasteFailed(QString)));

    QMetaObject::invokeMethod(pastePage_, "pasteFailed");
}

void LogicPasteApp::onOpenPaste(QString pasteUrl) {
    qDebug() << "onOpenPaste():" << pasteUrl;

    QmlDocument *qml = QmlDocument::create(this, "ViewPastePage.qml");
    if(!qml->hasErrors()) {
        Page *page = qml->createRootNode<Page>();
        qml->setContextProperty("cs", this);

        WebView *webView = page->findChild<WebView*>("webView");
        webView->setUrl(pasteUrl);

        navigationPane_->push(page);
    }
}
