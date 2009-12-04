/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QtGui>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkRequest>
#if !defined(QT_NO_PRINTER)
#include <QPrintPreviewDialog>
#endif

#ifndef QT_NO_UITOOLS
#include <QtUiTools/QUiLoader>
#endif

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QVector>

#include <cstdio>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebinspector.h>
#include <qwebpage.h>
#include <qwebsettings.h>
#include <qwebview.h>


#ifndef NDEBUG
void QWEBKIT_EXPORT qt_drt_garbageCollector_collect();
#endif

static QUrl urlFromUserInput(const QString& input)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    return QUrl::fromUserInput(input);
#else
    return QUrl(input);
#endif
}

class WebView : public QWebView {
    Q_OBJECT
public:
    WebView(QWidget* parent) : QWebView(parent) {}

protected:
    virtual void contextMenuEvent(QContextMenuEvent* event)
    {
        QMenu* menu = page()->createStandardContextMenu();

        QWebHitTestResult r = page()->mainFrame()->hitTestContent(event->pos());

        if (!r.linkUrl().isEmpty()) {
            QAction* newTabAction = menu->addAction(tr("Open in Default &Browser"), this, SLOT(openUrlInDefaultBrowser()));
            newTabAction->setData(r.linkUrl());
            menu->insertAction(menu->actions().at(2), newTabAction);
        }

        menu->exec(mapToGlobal(event->pos()));
        delete menu;
    }

    virtual void mousePressEvent(QMouseEvent* event)
    {
        mouseButtons = event->buttons();
        keyboardModifiers = event->modifiers();

        QWebView::mousePressEvent(event);
    }

public slots:
    void openUrlInDefaultBrowser(const QUrl &url = QUrl())
    {
        if (QAction* action = qobject_cast<QAction*>(sender()))
            QDesktopServices::openUrl(action->data().toUrl());
        else
            QDesktopServices::openUrl(url);
    }

public:
    Qt::MouseButtons mouseButtons;
    Qt::KeyboardModifiers keyboardModifiers;
};

class WebPage : public QWebPage {
public:
    WebPage(QWidget *parent) : QWebPage(parent) {}

    virtual QWebPage *createWindow(QWebPage::WebWindowType);
    virtual QObject* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&);
    virtual bool supportsExtension(QWebPage::Extension extension) const
    {
        if (extension == QWebPage::ErrorPageExtension)
            return true;
        return false;
    }
    virtual bool extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output);


    virtual bool acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest &request, NavigationType type)
    {
        WebView* webView = static_cast<WebView*>(view());
        if (webView->keyboardModifiers & Qt::ShiftModifier) {
            QWebPage* page = createWindow(QWebPage::WebBrowserWindow);
            page->mainFrame()->load(request);
            return false;
        }
        if (webView->keyboardModifiers & Qt::AltModifier) {
            webView->openUrlInDefaultBrowser(request.url());
            return false;
        }

        return QWebPage::acceptNavigationRequest(frame, request, type);
    }
};

class WebInspector : public QWebInspector {
    Q_OBJECT
public:
    WebInspector(QWidget* parent) : QWebInspector(parent) {}
signals:
    void visibleChanged(bool nowVisible);
protected:
    void showEvent(QShowEvent* event)
    {
        QWebInspector::showEvent(event);
        emit visibleChanged(true);
    }
    void hideEvent(QHideEvent* event)
    {
        QWebInspector::hideEvent(event);
        emit visibleChanged(false);
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QString url = QString()): currentZoom(100)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        if (qgetenv("QTLAUNCHER_USE_ARGB_VISUALS").toInt() == 1)
            setAttribute(Qt::WA_TranslucentBackground);

        QSplitter* splitter = new QSplitter(Qt::Vertical, this);
        setCentralWidget(splitter);

        view = new WebView(splitter);
        WebPage* page = new WebPage(view);
        view->setPage(page);
        connect(view, SIGNAL(loadFinished(bool)),
                this, SLOT(loadFinished()));
        connect(view, SIGNAL(titleChanged(const QString&)),
                this, SLOT(setWindowTitle(const QString&)));
        connect(view->page(), SIGNAL(linkHovered(const QString&, const QString&, const QString &)),
                this, SLOT(showLinkHover(const QString&, const QString&)));
        connect(view->page(), SIGNAL(windowCloseRequested()), this, SLOT(close()));

        inspector = new WebInspector(splitter);
        inspector->setPage(page);
        inspector->hide();
        connect(this, SIGNAL(destroyed()), inspector, SLOT(deleteLater()));

        setupUI();

        // set the proxy to the http_proxy env variable - if present
        QUrl proxyUrl = urlFromUserInput(qgetenv("http_proxy"));

        if (proxyUrl.isValid() && !proxyUrl.host().isEmpty()) {
            int proxyPort = (proxyUrl.port() > 0)  ? proxyUrl.port() : 8080;
            page->networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyPort));
        }

        QFileInfo fi(url);
        if (fi.exists() && fi.isRelative())
            url = fi.absoluteFilePath();

        QUrl qurl = urlFromUserInput(url);
        if (qurl.scheme().isEmpty())
            qurl = QUrl("http://" + url + "/");
        if (qurl.isValid()) {
            urlEdit->setText(qurl.toEncoded());
            view->load(qurl);
        }

        // the zoom values are chosen to be like in Mozilla Firefox 3
        zoomLevels << 30 << 50 << 67 << 80 << 90;
        zoomLevels << 100;
        zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;
    }

    QWebPage* webPage() const
    {
        return view->page();
    }

    QWebView* webView() const
    {
        return view;
    }

protected slots:

    void changeLocation()
    {
        QString string = urlEdit->text();
        QUrl url = urlFromUserInput(string);
        if (url.scheme().isEmpty())
            url = QUrl("http://" + string + "/");
        if (url.isValid()) {
            urlEdit->setText(url.toEncoded());
            view->load(url);
            view->setFocus(Qt::OtherFocusReason);
        }
    }

    void loadFinished()
    {
        urlEdit->setText(view->url().toString());

        QUrl::FormattingOptions opts;
        opts |= QUrl::RemoveScheme;
        opts |= QUrl::RemoveUserInfo;
        opts |= QUrl::StripTrailingSlash;
        QString s = view->url().toString(opts);
        s = s.mid(2);
        if (s.isEmpty())
            return;

        if (!urlList.contains(s))
            urlList += s;
        urlModel.setStringList(urlList);
    }

    void showLinkHover(const QString &link, const QString &toolTip)
    {
        statusBar()->showMessage(link);
#ifndef QT_NO_TOOLTIP
        if (!toolTip.isEmpty())
            QToolTip::showText(QCursor::pos(), toolTip);
#endif
    }

    void zoomIn()
    {
        int i = zoomLevels.indexOf(currentZoom);
        Q_ASSERT(i >= 0);
        if (i < zoomLevels.count() - 1)
            currentZoom = zoomLevels[i + 1];

        view->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void zoomOut()
    {
        int i = zoomLevels.indexOf(currentZoom);
        Q_ASSERT(i >= 0);
        if (i > 0)
            currentZoom = zoomLevels[i - 1];

        view->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void resetZoom()
    {
       currentZoom = 100;
       view->setZoomFactor(1.0);
    }

    void toggleZoomTextOnly(bool b)
    {
        view->page()->settings()->setAttribute(QWebSettings::ZoomTextOnly, b);
    }

    void print()
    {
#if !defined(QT_NO_PRINTER)
        QPrintPreviewDialog dlg(this);
        connect(&dlg, SIGNAL(paintRequested(QPrinter *)),
                view, SLOT(print(QPrinter *)));
        dlg.exec();
#endif
    }

    void screenshot()
    {
        QPixmap pixmap = QPixmap::grabWidget(view);
        QLabel* label = new QLabel;
        label->setAttribute(Qt::WA_DeleteOnClose);
        label->setWindowTitle("Screenshot - Preview");
        label->setPixmap(pixmap);
        label->show();

        QString fileName = QFileDialog::getSaveFileName(label, "Screenshot");
        if (!fileName.isEmpty()) {
            pixmap.save(fileName, "png");
            label->setWindowTitle(QString("Screenshot - Saved at %1").arg(fileName));
        }
    }

    void setEditable(bool on)
    {
        view->page()->setContentEditable(on);
        formatMenuAction->setVisible(on);
    }

    /*
    void dumpPlugins() {
        QList<QWebPluginInfo> plugins = QWebSettings::pluginDatabase()->plugins();
        foreach (const QWebPluginInfo plugin, plugins) {
            qDebug() << "Plugin:" << plugin.name();
            foreach (const QWebPluginInfo::MimeType mime, plugin.mimeTypes()) {
                qDebug() << "   " << mime.name;
            }
        }
    }
    */

    void dumpHtml()
    {
        qDebug() << "HTML: " << view->page()->mainFrame()->toHtml();
    }

    void selectElements()
    {
        bool ok;
        QString str = QInputDialog::getText(this, "Select elements", "Choose elements",
                                            QLineEdit::Normal, "a", &ok);

        if (ok && !str.isEmpty()) {
            QWebElementCollection result =  view->page()->mainFrame()->findAllElements(str);
            foreach (QWebElement e, result)
                e.setStyleProperty("background-color", "yellow");
            statusBar()->showMessage(QString("%1 element(s) selected").arg(result.count()), 5000);
        }
    }

public slots:

    void newWindow(const QString &url = QString())
    {
        MainWindow* mw = new MainWindow(url);
        mw->show();
    }

private:

    QVector<int> zoomLevels;
    int currentZoom;

    // create the status bar, tool bar & menu
    void setupUI()
    {
        progress = new QProgressBar(this);
        progress->setRange(0, 100);
        progress->setMinimumSize(100, 20);
        progress->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        progress->hide();
        statusBar()->addPermanentWidget(progress);

        connect(view, SIGNAL(loadProgress(int)), progress, SLOT(show()));
        connect(view, SIGNAL(loadProgress(int)), progress, SLOT(setValue(int)));
        connect(view, SIGNAL(loadFinished(bool)), progress, SLOT(hide()));

        urlEdit = new QLineEdit(this);
        urlEdit->setSizePolicy(QSizePolicy::Expanding, urlEdit->sizePolicy().verticalPolicy());
        connect(urlEdit, SIGNAL(returnPressed()),
                SLOT(changeLocation()));
        QCompleter* completer = new QCompleter(this);
        urlEdit->setCompleter(completer);
        completer->setModel(&urlModel);

        QToolBar* bar = addToolBar("Navigation");
        bar->addAction(view->pageAction(QWebPage::Back));
        bar->addAction(view->pageAction(QWebPage::Forward));
        bar->addAction(view->pageAction(QWebPage::Reload));
        bar->addAction(view->pageAction(QWebPage::Stop));
        bar->addWidget(urlEdit);

        QMenu* fileMenu = menuBar()->addMenu("&File");
        QAction* newWindow = fileMenu->addAction("New Window", this, SLOT(newWindow()));
        fileMenu->addAction(tr("Print"), this, SLOT(print()), QKeySequence::Print);
        QAction* screenshot = fileMenu->addAction("Screenshot", this, SLOT(screenshot()));
        fileMenu->addAction("Close", this, SLOT(close()));

        QMenu* editMenu = menuBar()->addMenu("&Edit");
        editMenu->addAction(view->pageAction(QWebPage::Undo));
        editMenu->addAction(view->pageAction(QWebPage::Redo));
        editMenu->addSeparator();
        editMenu->addAction(view->pageAction(QWebPage::Cut));
        editMenu->addAction(view->pageAction(QWebPage::Copy));
        editMenu->addAction(view->pageAction(QWebPage::Paste));
        editMenu->addSeparator();
        QAction* setEditable = editMenu->addAction("Set Editable", this, SLOT(setEditable(bool)));
        setEditable->setCheckable(true);

        QMenu* viewMenu = menuBar()->addMenu("&View");
        viewMenu->addAction(view->pageAction(QWebPage::Stop));
        viewMenu->addAction(view->pageAction(QWebPage::Reload));
        viewMenu->addSeparator();
        QAction* zoomIn = viewMenu->addAction("Zoom &In", this, SLOT(zoomIn()));
        QAction* zoomOut = viewMenu->addAction("Zoom &Out", this, SLOT(zoomOut()));
        QAction* resetZoom = viewMenu->addAction("Reset Zoom", this, SLOT(resetZoom()));
        QAction* zoomTextOnly = viewMenu->addAction("Zoom Text Only", this, SLOT(toggleZoomTextOnly(bool)));
        zoomTextOnly->setCheckable(true);
        zoomTextOnly->setChecked(false);
        viewMenu->addSeparator();
        viewMenu->addAction("Dump HTML", this, SLOT(dumpHtml()));
        //viewMenu->addAction("Dump plugins", this, SLOT(dumpPlugins()));

        QMenu* formatMenu = new QMenu("F&ormat", this);
        formatMenuAction = menuBar()->addMenu(formatMenu);
        formatMenuAction->setVisible(false);
        formatMenu->addAction(view->pageAction(QWebPage::ToggleBold));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleItalic));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleUnderline));
        QMenu* writingMenu = formatMenu->addMenu(tr("Writing Direction"));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionDefault));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionLeftToRight));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionRightToLeft));

        newWindow->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
        screenshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
        view->pageAction(QWebPage::Back)->setShortcut(QKeySequence::Back);
        view->pageAction(QWebPage::Stop)->setShortcut(Qt::Key_Escape);
        view->pageAction(QWebPage::Forward)->setShortcut(QKeySequence::Forward);
        view->pageAction(QWebPage::Reload)->setShortcut(QKeySequence::Refresh);
        view->pageAction(QWebPage::Undo)->setShortcut(QKeySequence::Undo);
        view->pageAction(QWebPage::Redo)->setShortcut(QKeySequence::Redo);
        view->pageAction(QWebPage::Cut)->setShortcut(QKeySequence::Cut);
        view->pageAction(QWebPage::Copy)->setShortcut(QKeySequence::Copy);
        view->pageAction(QWebPage::Paste)->setShortcut(QKeySequence::Paste);
        zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
        zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
        resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
        view->pageAction(QWebPage::ToggleBold)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
        view->pageAction(QWebPage::ToggleItalic)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
        view->pageAction(QWebPage::ToggleUnderline)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));

        QMenu* toolsMenu = menuBar()->addMenu("&Tools");
        toolsMenu->addAction("Select elements...", this, SLOT(selectElements()));
        QAction* showInspectorAction = toolsMenu->addAction("Show inspector", inspector, SLOT(setVisible(bool)));
        showInspectorAction->setCheckable(true);
        showInspectorAction->setShortcuts(QList<QKeySequence>() << QKeySequence(tr("F12")));
        showInspectorAction->connect(inspector, SIGNAL(visibleChanged(bool)), SLOT(setChecked(bool)));

    }

    QWebView* view;
    QLineEdit* urlEdit;
    QProgressBar* progress;
    WebInspector* inspector;

    QAction* formatMenuAction;

    QStringList urlList;
    QStringListModel urlModel;
};

bool WebPage::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    const QWebPage::ErrorPageExtensionOption* info = static_cast<const QWebPage::ErrorPageExtensionOption*>(option);
    QWebPage::ErrorPageExtensionReturn* errorPage = static_cast<QWebPage::ErrorPageExtensionReturn*>(output);

    errorPage->content = QString("<html><head><title>Failed loading page</title></head><body>%1</body></html>")
        .arg(info->errorString).toUtf8();

    return true;
}

QWebPage* WebPage::createWindow(QWebPage::WebWindowType)
{
    MainWindow* mw = new MainWindow;
    mw->show();
    return mw->webPage();
}

QObject* WebPage::createPlugin(const QString &classId, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    Q_UNUSED(url);
    Q_UNUSED(paramNames);
    Q_UNUSED(paramValues);
#ifndef QT_NO_UITOOLS
    QUiLoader loader;
    return loader.createWidget(classId, view());
#else
    Q_UNUSED(classId);
    return 0;
#endif
}

class URLLoader : public QObject {
    Q_OBJECT
public:
    URLLoader(QWebView* view, const QString& inputFileName)
        : m_view(view)
        , m_stdOut(stdout)
        , m_loaded(0)
    {
        init(inputFileName);
    }

public slots:
    void loadNext()
    {
        QString qstr;
        if (getUrl(qstr)) {
            QUrl url(qstr, QUrl::StrictMode);
            if (url.isValid()) {
                m_stdOut << "Loading " << qstr << " ......" << ++m_loaded << endl;
                m_view->load(url);
            } else
                loadNext();
        } else
            disconnect(m_view, 0, this, 0);
    }

private:
    void init(const QString& inputFileName)
    {
        QFile inputFile(inputFileName);
        if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&inputFile);
            QString line;
            while (true) {
                line = stream.readLine();
                if (line.isNull())
                    break;
                m_urls.append(line);
            }
        } else {
            qDebug() << "Cant't open list file";
            exit(0);
        }
        m_index = 0;
        inputFile.close();
    }

    bool getUrl(QString& qstr)
    {
        if (m_index == m_urls.size())
            return false;

        qstr = m_urls[m_index++];
        return true;
    }

private:
    QVector<QString> m_urls;
    int m_index;
    QWebView* m_view;
    QTextStream m_stdOut;
    int m_loaded;
};

#include "main.moc"

int launcherMain(const QApplication& app)
{
#ifndef NDEBUG
    int retVal = app.exec();
    qt_drt_garbageCollector_collect();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString defaultUrl = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));

    QWebSettings::setMaximumPagesInCache(4);

    app.setApplicationName("QtLauncher");
    app.setApplicationVersion("0.1");

    QWebSettings::setObjectCacheCapacities((16*1024*1024) / 8, (16*1024*1024) / 8, 16*1024*1024);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    QWebSettings::enablePersistentStorage();

    // To allow QWebInspector's configuration persistence
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setApplicationName("QtLauncher");

    const QStringList args = app.arguments();

    if (args.contains(QLatin1String("-r"))) {
        // robotized
        QString listFile = args.at(2);
        if (!(args.count() == 3) && QFile::exists(listFile)) {
            qDebug() << "Usage: QtLauncher -r listfile";
            exit(0);
        }
        MainWindow* window = new MainWindow;
        QWebView* view = window->webView();
        URLLoader loader(view, listFile);
        QObject::connect(view, SIGNAL(loadFinished(bool)), &loader, SLOT(loadNext()));
        loader.loadNext();
        window->show();
        launcherMain(app);
    } else {
        MainWindow* window = 0;

        // Look though the args for something we can open
        for (int i = 1; i < args.count(); i++) {
            if (!args.at(i).startsWith("-")) {
                if (!window)
                    window = new MainWindow(args.at(i));
                else
                    window->newWindow(args.at(i));
            }
        }

        // If not, just open the default URL
        if (!window)
            window = new MainWindow(defaultUrl);

        window->show();
        launcherMain(app);
    }
}
