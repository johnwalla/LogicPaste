#include "ViewPastePage.h"

#include <bb/cascades/QmlDocument>
#include <bb/cascades/Page>
#include <bb/cascades/TitleBar>
#include <bb/cascades/WebView>
#include <bb/cascades/WebSettings>
#include <bb/system/Clipboard>

#include <bps/navigator.h>

#include "PasteModel.h"
#include "Pastebin.h"
#include "PasteFormatter.h"

ViewPastePage::ViewPastePage(PasteModel *pasteModel, const QString &pasteKey, QObject *parent)
    : QObject(parent), pasteModel_(pasteModel)
{
    QmlDocument *qml = QmlDocument::create("ViewPastePage.qml");
    qml->setContextProperty("cs", this);
    root_ = qml->createRootNode<Page>();
    connect(root_, SIGNAL(destroyed()), this, SLOT(deleteLater()));

    findAndConnectControls();

    pasteListing_ = pasteModel_->pasteListing(pasteKey);
    if(!pasteListing_.title().isEmpty()) {
        root_->titleBar()->setTitle(pasteListing_.title());
    }

    connect(pasteModel_, SIGNAL(pasteAvailable(PasteListing, QByteArray)), this, SLOT(onPasteAvailable(PasteListing, QByteArray)));
    connect(pasteModel_, SIGNAL(pasteError(PasteListing)), this, SLOT(onPasteError(PasteListing)));

    pasteModel_->requestPaste(pasteKey);
}

ViewPastePage::~ViewPastePage()
{
}

void ViewPastePage::findAndConnectControls()
{
    connect(root_, SIGNAL(editPaste()), this, SLOT(onEditPaste()));
    connect(root_, SIGNAL(openInBrowser()), this, SLOT(onOpenInBrowser()));
    connect(root_, SIGNAL(copyPaste()), this, SLOT(onCopyPaste()));
    connect(root_, SIGNAL(copyUrl()), this, SLOT(onCopyUrl()));

    webView_ = root_->findChild<WebView*>("webView");
}

Page* ViewPastePage::rootNode() const
{
    return root_;
}

void ViewPastePage::onPasteAvailable(PasteListing pasteListing, QByteArray rawPaste)
{
    if(pasteListing.key() != pasteListing_.key()) { return; }
    disconnect(pasteModel_, SIGNAL(pasteAvailable(PasteListing, QByteArray)), this, SLOT(onPasteAvailable(PasteListing, QByteArray)));
    disconnect(pasteModel_, SIGNAL(pasteError(PasteListing)), this, SLOT(onPasteError(PasteListing)));
    pasteListing_ = pasteListing;
    rawPaste_ = rawPaste;

    PasteFormatter *formatter = new PasteFormatter(this);
    connect(formatter, SIGNAL(pasteFormatted(const QString&, const QString&)),
        this, SLOT(onPasteFormatted(const QString&, const QString&)));
    connect(formatter, SIGNAL(formatError()), this, SLOT(onFormatError()));
    formatter->formatPaste(pasteListing.key(), pasteListing.formatShort(), rawPaste);
}

void ViewPastePage::onPasteError(PasteListing pasteListing)
{
    if(pasteListing.key() != pasteListing_.key()) { return; }
    disconnect(pasteModel_, SIGNAL(pasteAvailable(PasteListing, QByteArray)), this, SLOT(onPasteAvailable(PasteListing, QByteArray)));
    disconnect(pasteModel_, SIGNAL(pasteError(PasteListing)), this, SLOT(onPasteError(PasteListing)));
    pasteListing_ = pasteListing;

    QString errorHtml = QString("<html><body>%1</body></html>").arg(tr("Error retrieving paste"));
    webView_->settings()->setMinimumFontSize(36);
    webView_->setHtml(errorHtml);
}

void ViewPastePage::onPasteFormatted(const QString& pasteKey, const QString& html)
{
    PasteFormatter *formatter = qobject_cast<PasteFormatter*>(sender());
    Q_UNUSED(pasteKey);
    disconnect(formatter, SIGNAL(pasteFormatted(const QString&, const QString&)),
        this, SLOT(onPasteFormatted(const QString&, const QString&)));
    disconnect(formatter, SIGNAL(formatError()), this, SLOT(onFormatError()));

    webView_->settings()->setMinimumFontSize(36);
    webView_->setHtml(html);
    root_->setProperty("pasteLoaded", true);
}

void ViewPastePage::onFormatError()
{
    PasteFormatter *formatter = qobject_cast<PasteFormatter*>(sender());
    disconnect(formatter, SIGNAL(pasteFormatted(const QString&, const QString&)),
        this, SLOT(onPasteFormatted(const QString&, const QString&)));
    disconnect(formatter, SIGNAL(formatError()), this, SLOT(onFormatError()));

    QString errorHtml = QString("<html><body>%1</body></html>").arg(tr("Error formatting paste"));
    webView_->settings()->setMinimumFontSize(36);
    webView_->setHtml(errorHtml);
}

void ViewPastePage::onEditPaste() {
    emit editPaste(pasteListing_, rawPaste_);
}

void ViewPastePage::onOpenInBrowser()
{
    navigator_invoke(pasteListing_.url().toLatin1(), 0);
}

void ViewPastePage::onCopyPaste()
{
    if(rawPaste_.isEmpty()) { return; }
    bb::system::Clipboard clipboard;
    if(clipboard.clear()) {
        clipboard.insert("text/plain", rawPaste_);
    }
}

void ViewPastePage::onCopyUrl()
{
    bb::system::Clipboard clipboard;
    if(clipboard.clear()) {
        clipboard.insert("text/plain", pasteListing_.url().toUtf8());
    }
}