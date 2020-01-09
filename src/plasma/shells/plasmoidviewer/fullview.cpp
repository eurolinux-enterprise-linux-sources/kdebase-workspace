/*
 * Copyright 2007 Frerich Raabe <raabe@kde.org>
 * Copyright 2007 Aaron Seigo <aseigo@kde.org>
 * Copyright 2008 Aleix Pol <aleixpol@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "fullview.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QResizeEvent>

#include <KIconLoader>
#include <KStandardDirs>

#include <Plasma/Containment>
#include <Plasma/Wallpaper>

using namespace Plasma;

FullView::FullView(const QString &ff, const QString &loc, QWidget *parent)
    : QGraphicsView(parent),
      m_formfactor(Plasma::Planar),
      m_location(Plasma::Floating),
      m_containment(0),
      m_applet(0)
{
    setFrameStyle(QFrame::NoFrame);
    QString formfactor = ff.toLower();
    if (formfactor.isEmpty() || formfactor == "planar") {
        m_formfactor = Plasma::Planar;
    } else if (formfactor == "vertical") {
        m_formfactor = Plasma::Vertical;
    } else if (formfactor == "horizontal") {
        m_formfactor = Plasma::Horizontal;
    } else if (formfactor == "mediacenter") {
        m_formfactor = Plasma::MediaCenter;
    }

    QString location = loc.toLower();
    if (loc.isEmpty() || loc == "floating") {
        m_location = Plasma::Floating;
    } else if (loc == "desktop") {
        m_location = Plasma::Desktop;
    } else if (loc == "fullscreen") {
        m_location = Plasma::FullScreen;
    } else if (loc == "top") {
        m_location = Plasma::TopEdge;
    } else if (loc == "bottom") {
        m_location = Plasma::BottomEdge;
    } else if (loc == "right") {
        m_location = Plasma::RightEdge;
    } else if (loc == "left") {
        m_location = Plasma::LeftEdge;
    }

    resize(250, 200);
    setScene(&m_corona);
    connect(&m_corona, SIGNAL(sceneRectChanged(QRectF)), this, SLOT(sceneRectChanged(QRectF)));
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

void FullView::addApplet(const QString &name, const QString &containment,
                         const QString& wallpaper, const QVariantList &args)
{
    kDebug() << "adding applet" << name << "in" << containment;
    m_containment = m_corona.addContainment(containment);
    connect(m_containment, SIGNAL(appletRemoved(Plasma::Applet*)), this, SLOT(appletRemoved()));

    if (!wallpaper.isEmpty()) {
        m_containment->setWallpaper(wallpaper);
    }

    m_containment->setFormFactor(m_formfactor);
    m_containment->setLocation(m_location);
    m_containment->resize(size());
    setScene(m_containment->scene());

    QFileInfo info(name);
    if (!info.isAbsolute()) {
        info = QFileInfo(QDir::currentPath() + "/" + name);
    }

    if (info.exists()) {
        m_applet = Applet::loadPlasmoid(info.absoluteFilePath());
    }

    if (!m_applet) {
        m_applet = m_containment->addApplet(name, args, QRectF(0, 0, -1, -1));
    } else {
        m_containment->addApplet(m_applet, QPointF(-1, -1), false);
    }

    m_applet->setFlag(QGraphicsItem::ItemIsMovable, false);
    setSceneRect(m_containment->geometry());
    setWindowTitle(m_applet->name());
    setWindowIcon(SmallIcon(m_applet->icon()));
}

void FullView::appletRemoved()
{
    m_applet = 0;
}

void FullView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);

    if (!m_applet) {
        kDebug() << "no applet";
        return;
    }

    m_containment->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    m_containment->setMinimumSize(size());
    m_containment->setMaximumSize(size());
    m_containment->resize(size());
    if (m_containment->layout()) {
        return;
    }

    //kDebug() << size();
    qreal newWidth = 0;
    qreal newHeight = 0;

    if (false && m_applet->aspectRatioMode() == Plasma::KeepAspectRatio) {
        // The applet always keeps its aspect ratio, so let's respect it.
        qreal ratio = m_applet->size().width() / m_applet->size().height();
        qreal widthForCurrentHeight = (qreal)size().height() * ratio;
        if (widthForCurrentHeight > size().width()) {
            newHeight = size().width() / ratio;
            newWidth = newHeight * ratio;
        } else {
            newWidth = widthForCurrentHeight;
            newHeight = newWidth / ratio;
        }
    } else {
        newWidth = size().width();
        newHeight = size().height();
    }
    QSizeF newSize(newWidth, newHeight);

    // check if the rect is valid, or else it seems to try to allocate
    // up to infinity memory in exponential increments
    if (newSize.isValid()) {
        m_applet->resize(QSizeF(newWidth, newHeight));
    }
}

void FullView::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    qApp->quit();
}

void FullView::sceneRectChanged(const QRectF &rect)
{
    Q_UNUSED(rect)
    if (m_applet) {
        setSceneRect(m_applet->geometry());
    }
    return;
    if (m_containment && m_containment->layout()) {
        setSceneRect(m_containment->geometry());
    } else if (m_applet) {
        //kDebug() << m_applet->geometry();
        setSceneRect(m_applet->geometry());
    }
}

#include "fullview.moc"

