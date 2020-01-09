/***************************************************************************
 *   fdoprotocol.cpp                                                       *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
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
 ***************************************************************************/

#include "dbusnotification.h"
#include "dbusnotificationprotocol.h"

#include <KConfigGroup>
#include <KIcon>

#include <plasma/dataenginemanager.h>
#include <plasma/service.h>


namespace SystemTray
{

static const char *engineName = "notifications";

DBusNotificationProtocol::DBusNotificationProtocol(QObject *parent)
    : Protocol(parent),
      m_engine(0)
{
}


DBusNotificationProtocol::~DBusNotificationProtocol()
{
    if (m_engine) {
        Plasma::DataEngineManager::self()->unloadEngine(engineName);
    }
}


void DBusNotificationProtocol::init()
{
    m_engine = Plasma::DataEngineManager::self()->loadEngine(engineName);

    if (!m_engine->isValid()) {
        m_engine = 0;
        return;
    }

    connect(m_engine, SIGNAL(sourceAdded(const QString&)),
            this, SLOT(prepareNotification(const QString&)));
    connect(m_engine, SIGNAL(sourceRemoved(const QString&)),
            this, SLOT(removeNotification(const QString&)));
}


void DBusNotificationProtocol::prepareNotification(const QString &source)
{
    m_engine->connectSource(source, this);
}


void DBusNotificationProtocol::dataUpdated(const QString &source, const Plasma::DataEngine::Data &data)
{
    bool isNew = !m_notifications.contains(source);

    if (isNew) {
        DBusNotification * notification = new DBusNotification(source, this);
        connect(notification, SIGNAL(unregisterNotification(const QString&)),
                this, SLOT(unregisterNotification(const QString&)));
        connect(notification, SIGNAL(notificationDeleted(const QString&)),
                this, SLOT(notificationDeleted(const QString&)));
        connect(notification, SIGNAL(actionTriggered(const QString&, const QString&)),
                this, SLOT(relayAction(const QString&, const QString&)));
        m_notifications[source] = notification;
    }

    DBusNotification* notification = m_notifications[source];
    notification->setApplicationName(data.value("appName").toString());
    notification->setApplicationIcon(KIcon(data.value("appIcon").toString()));
    notification->setEventId(data.value("eventId").toString());
    notification->setSummary(data.value("summary").toString());
    notification->setMessage(data.value("body").toString());
    notification->setTimeout(data.value("expireTimeout").toInt());

    if (data.contains("image")) {
        QImage image = qvariant_cast<QImage>(data.value("image"));
        notification->setImage(image);
    }

    QStringList codedActions = data.value("actions").toStringList();
    if (codedActions.count() % 2 != 0) {
        kDebug() << "Invalid actions" << codedActions << "from" << notification->applicationName();
        codedActions.clear();
    }

    QHash<QString, QString> actions;
    QStringList actionOrder;

    while (!codedActions.isEmpty()) {
        QString actionId = codedActions.takeFirst();
        QString actionName = codedActions.takeFirst();
        actions.insert(actionId, actionName);
        actionOrder.append(actionId);
    }

    notification->setActions(actions);
    notification->setActionOrder(actionOrder);

    if (isNew) {
        emit notificationCreated(notification);
    } else {
        emit notification->changed(notification);
    }
}


void DBusNotificationProtocol::relayAction(const QString &source, const QString &actionId)
{
    Plasma::Service *service = m_engine->serviceForSource(source);
    KConfigGroup op = service->operationDescription("invokeAction");

    if (op.isValid()) {
        op.writeEntry("actionId", actionId);
        service->startOperationCall(op);
    } else {
        kDebug() << "invalid operation";
    }
}

void DBusNotificationProtocol::unregisterNotification(const QString &source)
{
    Plasma::Service *service = m_engine->serviceForSource(source);
    KConfigGroup op = service->operationDescription("userClosed");
    service->startOperationCall(op);
}

void DBusNotificationProtocol::removeNotification(const QString &source)
{
    if (m_notifications.contains(source)) {
        m_notifications.take(source)->deleteLater();
    }
}

void DBusNotificationProtocol::notificationDeleted(const QString &source)
{
    m_notifications.remove(source);
}

}

#include "dbusnotificationprotocol.moc"
