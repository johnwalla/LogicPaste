#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QSocketNotifier>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSsl>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QNetworkDiskCache>
#include <bb/system/SystemDialog>
#include <bb/system/SystemUiResult>

#include "Pastebin.h"
#include "PasteListing.h"
#include "AppSettings.h"
#include "config.h"

namespace {
const char* const PASTEBIN_DEV_KEY = PASTEBIN_API_KEY;
const char* const URLENCODED_CONTENT_TYPE = "application/x-www-form-urlencoded";
const QString HTTPS_SCHEME("https");
}

class PasteUserData {
public:
    PasteUserData() : accountType(AppSettings::Normal), pasteVisibility(PasteListing::Public) { }
    QString username;
    QString avatarUrl;
    QString website;
    QString email;
    QString location;
    AppSettings::AccountType accountType;
    QString pasteFormatShort;
    QString pasteExpiration;
    PasteListing::Visibility pasteVisibility;
};

Pastebin::Pastebin(QObject *parent)
    : QObject(parent) {

    loadRootCert("app/native/assets/models/PositiveSSL.ca-1");
    loadRootCert("app/native/assets/models/PositiveSSL.ca-2");

    QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
    diskCache->setCacheDirectory("data/cache");
    accessManager_.setCache(diskCache);

    connect(&accessManager_, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
        this, SLOT(onSslErrors(QNetworkReply*,QList<QSslError>)));
}

Pastebin::~Pastebin() {

}

void Pastebin::loadRootCert(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open PositiveSSL CA certificate";
    }

    QSslCertificate cert(&file, QSsl::Pem);
    if(cert.isValid() && !cert.isNull()) {
        QSslSocket::addDefaultCaCertificate(cert);
    }
    else {
        qWarning() << "Could not load PositiveSSL CA certificate";
    }
}

void Pastebin::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    QStringList ignoreCerts = AppSettings::instance()->ignoreErrorCerts();

    QList<QSslError> ignoreErrors;
    QList<QSslError> promptErrors;
    QSslCertificate cert;
    QStringList errorStrings;
    foreach(const QSslError &error, errors) {
        if(ignoreCerts.contains(QString(error.certificate().serialNumber()))) {
            ignoreErrors.append(error);
        }
        else {
            promptErrors.append(error);
            if(cert.isNull()) {
                cert = error.certificate();
            }
            errorStrings << error.errorString();
        }
    }

    if(!ignoreErrors.isEmpty()) {
        reply->ignoreSslErrors(ignoreErrors);
    }

    if(!promptErrors.isEmpty()) {
        QString bodyText = tr(
            "Issued to: %1\n"
            "Serial number: %2\n"
            "Issued by: %3\n"
            "Effective: %4\n"
            "Expires: %5\n"
            "\n%6\n\n"
            "Ignore this error?")
            .arg(cert.subjectInfo(QSslCertificate::CommonName))
            .arg(QString(cert.serialNumber()))
            .arg(cert.issuerInfo(QSslCertificate::CommonName))
            .arg(cert.effectiveDate().toLocalTime().toString(Qt::SystemLocaleShortDate))
            .arg(cert.expiryDate().toLocalTime().toString(Qt::SystemLocaleShortDate))
            .arg(errorStrings.join("\n"));

        bb::system::SystemDialog dialog(tr("Yes"), tr("Always"), tr("No"));
        dialog.setTitle(tr("SSL Error"));
        dialog.setBody(bodyText);
        bb::system::SystemUiResult::Type result = dialog.exec();

        if(result == bb::system::SystemUiResult::ConfirmButtonSelection
            || result == bb::system::SystemUiResult::CustomButtonSelection) {
            reply->ignoreSslErrors(promptErrors);

            if(result == bb::system::SystemUiResult::CustomButtonSelection) {
                ignoreCerts << QString(cert.serialNumber());
                AppSettings::instance()->setIgnoreErrorCerts(ignoreCerts);
            }
        }
    }
}

QUrl Pastebin::buildUrl(const QString &baseUrl)
{
    QUrl url(baseUrl);
    if(AppSettings::instance()->useSsl()) {
        url.setScheme(HTTPS_SCHEME);
    }
    return url;
}

void Pastebin::login(const QString& username, const QString& password) {
    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_login.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_user_name", username.toUtf8());
    params.addQueryItem("api_user_password", password.toUtf8());

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onLoginFinished()));
}

void Pastebin::onLoginFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << "Login complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        QString response = networkReply->readAll();

        qDebug() << "Response:" << response;

        if(response.startsWith("Bad API request")) {
            emit loginFailed(response);
        }
        else {
            QNetworkRequest networkRequest = networkReply->request();

            emit loginComplete(response);
        }
    }
    else {
        qWarning() << "Error:" << networkReply->errorString();
        emit loginFailed(QString::null);
    }
}

void Pastebin::submitPaste(const QString& pasteContent, const QString& pasteTitle, const QString& format, const QString& expiration, const PasteListing::Visibility visibility) {
    qDebug() << "submitPaste()";

    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_post.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_user_key", apiKey());
    params.addQueryItem("api_option", "paste");
    params.addQueryItem("api_paste_code", pasteContent);
    params.addQueryItem("api_paste_private", QString("%1").arg(static_cast<int>(visibility)));
    if(!pasteTitle.isEmpty()) {
        params.addQueryItem("api_paste_name", pasteTitle);
    }
    if(!expiration.isEmpty()) {
        params.addQueryItem("api_paste_expire_date", expiration);
    }
    if(!format.isEmpty()) {
        params.addQueryItem("api_paste_format", format);
    }

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onSubmitPasteFinished()));
}

void Pastebin::requestUserDetails() {
    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_post.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_user_key", apiKey());
    params.addQueryItem("api_option", "userdetails");

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onUserDetailsFinished()));
}

void Pastebin::requestHistory() {
    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_post.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_user_key", apiKey());
    params.addQueryItem("api_option", "list");

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onHistoryFinished()));
}

void Pastebin::requestTrending() {
    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_post.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_option", "trends");

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onTrendingFinished()));
}

void Pastebin::onSubmitPasteFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << "Paste complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        const QString response = networkReply->readAll();

        qDebug() << "Response:" << response;

        if(response.startsWith("Bad API request")) {
            qWarning() << "Error with paste";
            emit pasteFailed(response);
        }
        else {
            qDebug() << "Paste successful";
            emit pasteComplete(response);
        }
    }
    else {
        qWarning() << "Error with paste:" << networkReply->errorString();
        emit pasteFailed(QString::null);
    }
}

void Pastebin::onUserDetailsFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << "User details request complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        const QString response = networkReply->readAll();

        if(response.startsWith("Bad API request")) {
            qWarning() << "Error fetching user details:" << response;
            emit userDetailsError(response);
        }
        else {
            QXmlStreamReader reader;
            reader.addData("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n");
            reader.addData("<response>");
            reader.addData(response);
            reader.addData("</response>");

            bool success = false;

            PasteUserData pasteUser;
            while(!reader.atEnd()) {
                QXmlStreamReader::TokenType token = reader.readNext();
                if(token == QXmlStreamReader::StartDocument) {
                    qDebug() << "StartDocument";
                    continue;
                }
                else if(token == QXmlStreamReader::StartElement) {
                    if(reader.name() == "user") {
                        parseUserDetails(reader, &pasteUser);
                        success = true;
                    }
                }
                else if(token == QXmlStreamReader::EndDocument) {
                    qDebug() << "EndDocument";
                    continue;
                }
            }

            qDebug() << "Parsed user details";

            if(!success || reader.hasError()) {
                qWarning() << "Parse error:" << reader.errorString();
                emit userDetailsError(QString::null);
            } else {
                AppSettings *appSettings = AppSettings::instance();
                appSettings->setUsername(pasteUser.username);
                appSettings->setAvatarUrl(pasteUser.avatarUrl);
                appSettings->setWebsite(pasteUser.website);
                appSettings->setEmail(pasteUser.email);
                appSettings->setLocation(pasteUser.location);
                appSettings->setAccountType(pasteUser.accountType);
                appSettings->setPasteFormatShort(pasteUser.pasteFormatShort);
                appSettings->setPasteExpiration(pasteUser.pasteExpiration);
                appSettings->setPasteVisibility(pasteUser.pasteVisibility);
                appSettings->setApiKey(apiKey());
                emit userDetailsUpdated();

                requestUserAvatar();
            }
        }
    }
    else {
        qWarning() << "Error in user details response:" << networkReply->errorString();
        emit userDetailsError(QString::null);
    }
}

void Pastebin::parseUserDetails(QXmlStreamReader& reader, PasteUserData *pasteUser) {
    qDebug() << "parseUserDetails()";
    while(reader.readNext() != QXmlStreamReader::EndElement) {
        if(reader.name() == "user_name") {
            pasteUser->username = reader.readElementText();
        }
        else if(reader.name() == "user_format_short") {
            pasteUser->pasteFormatShort = reader.readElementText();
        }
        else if(reader.name() == "user_expiration") {
            pasteUser->pasteExpiration = reader.readElementText();
        }
        else if(reader.name() == "user_avatar_url") {
            pasteUser->avatarUrl = reader.readElementText();
        }
        else if(reader.name() == "user_private") {
            int value = reader.readElementText().toInt();
            if(value == 0) {
                pasteUser->pasteVisibility = PasteListing::Public;
            }
            else if(value == 1) {
                pasteUser->pasteVisibility = PasteListing::Unlisted;
            }
            else if(value == 2) {
                pasteUser->pasteVisibility = PasteListing::Private;
            }
        }
        else if(reader.name() == "user_website") {
            pasteUser->website = reader.readElementText();
        }
        else if(reader.name() == "user_email") {
            pasteUser->email = reader.readElementText();
        }
        else if(reader.name() == "user_location") {
            pasteUser->location = reader.readElementText();
        }
        else if(reader.name() == "user_account_type") {
            int value = reader.readElementText().toInt();
            if(value == 0) {
                pasteUser->accountType = AppSettings::Normal;
            }
            else if(value == 1) {
                pasteUser->accountType = AppSettings::Pro;
            }
        }
    }
}

void Pastebin::requestUserAvatar()
{
    const QString avatarUrl = AppSettings::instance()->avatarUrl();
    if(avatarUrl.isEmpty()) { return; }

    const QUrl url(avatarUrl);
    QNetworkRequest request(url);

    QNetworkReply *reply = accessManager_.get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onUserAvatarFinished()));
}

void Pastebin::onUserAvatarFinished()
{
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << "User avatar request complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        const QByteArray data = networkReply->readAll();
        if(!data.isEmpty()) {
            AppSettings::instance()->setAvatarImage(data);
            emit userAvatarUpdated();
        }
    }
    else {
        qWarning() << "Error fetching avatar:" << networkReply->errorString();
    }
}

void Pastebin::onHistoryFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QList<PasteListing> *pasteList = new QList<PasteListing>();
    if(processPasteListResponse(networkReply, pasteList)) {
        emit historyAvailable(pasteList);
    }
    else {
        delete pasteList;
        emit historyError();
    }
}

void Pastebin::onTrendingFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QList<PasteListing> *pasteList = new QList<PasteListing>();
    if(processPasteListResponse(networkReply, pasteList)) {
        emit trendingAvailable(pasteList);
    }
    else {
        delete pasteList;
        emit trendingError();
    }
}

bool Pastebin::processPasteListResponse(QNetworkReply *networkReply, QList<PasteListing> *pasteList) {
    bool result;
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << "Paste list request complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        QXmlStreamReader reader;
        // Wrap the server response in the necessary bits to make it more
        // closely resemble a valid XML document.  This is necessary because
        // the Pastebin API simply returns an unenclosed list of XML fragments.
        reader.addData("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n");
        reader.addData("<response>");
        reader.addData(networkReply->readAll());
        reader.addData("</response>");

        result = parsePasteList(reader, pasteList);
    }
    else {
        qWarning() << "Error in paste list response:" << networkReply->errorString();
        result = false;
    }
    return result;
}

bool Pastebin::parsePasteList(QXmlStreamReader& reader, QList<PasteListing> *pasteList) {
    while(!reader.atEnd()) {
        QXmlStreamReader::TokenType token = reader.readNext();
        if(token == QXmlStreamReader::StartDocument) {
            qDebug() << "StartDocument";
            continue;
        }
        else if(token == QXmlStreamReader::StartElement) {
            if(reader.name() == "paste") {
                parsePasteElement(reader, pasteList);
            }
        }
        else if(token == QXmlStreamReader::EndDocument) {
            qDebug() << "EndDocument";
            continue;
        }
    }

    qDebug() << "Parsed" << pasteList->size() << "paste elements";

    if(reader.hasError()) {
        qWarning() << "Parse error:" << reader.errorString();
        return false;
    }
    else {
        return true;
    }
}

void Pastebin::parsePasteElement(QXmlStreamReader& reader, QList<PasteListing> *pasteList) {
    qDebug() << "parsePasteElement()";
    PasteListing paste;
    while(reader.readNext() != QXmlStreamReader::EndElement) {
        if(reader.name() == "paste_key") {
            paste.setKey(reader.readElementText());
        }
        else if(reader.name() == "paste_date") {
            QString dateText = reader.readElementText();
            paste.setPasteDate(QDateTime::fromTime_t(dateText.toLongLong()));
        }
        else if(reader.name() == "paste_title") {
            paste.setTitle(reader.readElementText());
        }
        else if(reader.name() == "paste_size") {
            paste.setPasteSize(reader.readElementText().toInt());
        }
        else if(reader.name() == "paste_expire_date") {
            paste.setExpireDate(QDateTime::fromMSecsSinceEpoch(reader.readElementText().toULongLong()));
        }
        else if(reader.name() == "paste_private") {
            int value = reader.readElementText().toInt();
            if(value == 0) {
                paste.setVisibility(PasteListing::Public);
            }
            else if(value == 1) {
                paste.setVisibility(PasteListing::Unlisted);
            }
            else if(value == 2) {
                paste.setVisibility(PasteListing::Private);
            }
        }
        else if(reader.name() == "paste_format_long") {
            paste.setFormatLong(reader.readElementText());
        }
        else if(reader.name() == "paste_format_short") {
            paste.setFormatShort(reader.readElementText());
        }
        else if(reader.name() == "paste_url") {
            paste.setUrl(reader.readElementText());
        }
        else if(reader.name() == "paste_hits") {
            paste.setHits(reader.readElementText().toInt());
        }
    }
    pasteList->append(paste);
}

void Pastebin::requestRawPaste(const QString& pasteKey) {
    qDebug().nospace() << "requestRawPaste(" << pasteKey << ")";
    QUrl url("http://pastebin.com/raw.php");
    url.addQueryItem("i", pasteKey);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), pasteKey);
    QNetworkReply *reply = accessManager_.get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRequestRawPasteFinished()));
}

void Pastebin::onRequestRawPasteFinished() {
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QNetworkRequest networkRequest = networkReply->request();
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QString pasteKey = networkRequest.attribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1)).toString();
    qDebug() << "Paste request complete:" << statusCode.toInt();
    networkReply->deleteLater();

    if(networkReply->error() == QNetworkReply::NoError) {
        const QByteArray data = networkReply->readAll();

        if(!data.isEmpty()) {
            emit rawPasteAvailable(pasteKey, data);
        } else {
            emit rawPasteError(pasteKey);
        }
    } else {
        qWarning() << "Error requesting raw paste:" << networkReply->errorString();
        emit rawPasteError(pasteKey);
    }
}

void Pastebin::requestDeletePaste(const QString& pasteKey)
{
    qDebug().nospace() << "requestDeletePaste(" << pasteKey << ")";

    QNetworkRequest request(buildUrl("http://pastebin.com/api/api_post.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, URLENCODED_CONTENT_TYPE);
    request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), pasteKey);

    QUrl params;
    params.addQueryItem("api_dev_key", PASTEBIN_DEV_KEY);
    params.addQueryItem("api_user_key", apiKey());
    params.addQueryItem("api_paste_key", pasteKey);
    params.addQueryItem("api_option", "delete");

    QNetworkReply *reply = accessManager_.post(request, params.encodedQuery());
    connect(reply, SIGNAL(finished()), this, SLOT(onDeletePasteFinished()));
}

void Pastebin::onDeletePasteFinished()
{
    QNetworkReply *networkReply = qobject_cast<QNetworkReply *>(sender());
    QNetworkRequest networkRequest = networkReply->request();
    QVariant statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QString pasteKey = networkRequest.attribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1)).toString();
    qDebug() << "Delete paste complete:" << statusCode.toInt();

    if(networkReply->error() == QNetworkReply::NoError) {
        QString response = networkReply->readAll();

        qDebug() << "Response:" << response;

        if(response.startsWith("Bad API request")) {
            qWarning() << "Error with delete paste";
            emit deletePasteError(pasteKey, response);
        }
        else {
            qDebug() << "Delete paste successful";
            emit deletePasteComplete(pasteKey);
        }
    }
    else {
        qWarning() << "Error with delete paste" << networkReply->errorString();
        emit deletePasteError(pasteKey, QString::null);
    }
}

void Pastebin::setApiKey(const QString& apiKey) {
    apiKey_ = apiKey;
}

QString Pastebin::apiKey() const {
    return apiKey_;
}
