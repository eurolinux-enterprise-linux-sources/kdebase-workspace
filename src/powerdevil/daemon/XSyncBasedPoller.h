/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 **************************************************************************/

#ifndef XSYNCBASEDPOLLER_H
#define XSYNCBASEDPOLLER_H

#include "AbstractSystemPoller.h"

#include <config-X11.h>

#include <KApplication>
#include <KDebug>
#include <QDataStream>
#include <QWidget>

#ifdef HAVE_XSYNC
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#endif

class XSyncBasedPoller : public AbstractSystemPoller
{
    Q_OBJECT

public:

    static XSyncBasedPoller *instance();

    XSyncBasedPoller(QObject *parent = 0);
    virtual ~XSyncBasedPoller();

    AbstractSystemPoller::PollingType getPollingType() {
        return AbstractSystemPoller::XSyncBased;
    };
    QString name();

    bool isAvailable();
    bool setUpPoller();
    void unloadPoller();

protected:
    bool x11Event(XEvent *event);

public slots:
    void setNextTimeout(int nextTimeout);
    void forcePollRequest();
    void stopCatchingTimeouts();
    void catchIdleEvent();
    void stopCatchingIdleEvents();

private slots:
    void poll();

signals:
    void resumingFromIdle();
    void pollRequest(int idleTime);

#ifdef HAVE_XSYNC
private:
    void setAlarm(Display *dpy, XSyncAlarm *alarm, XSyncCounter counter,
                  XSyncTestType test, XSyncValue value);
#endif

private:
#ifdef HAVE_XSYNC
    Display * m_display;
    int                 m_sync_event, m_sync_error;
    XSyncSystemCounter  *m_counters;
    XSyncCounter        m_idleCounter;
    XSyncAlarm          m_timeoutAlarm;
    XSyncAlarm          m_resetAlarm;
#endif
    QWidget * m_filterWidget;
    bool m_available;
};

#endif /* XSYNCBASEDPOLLER_H */


