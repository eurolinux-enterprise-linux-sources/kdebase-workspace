/*  This file is part of the KDE project
    Copyright (C) 2007 Will Stephenson <wstephenson@kde.org>
    Copyright (C) 2008 Pino Toscano <pino@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef NETWORKMANAGER_NETWORKINTERFACE_H
#define NETWORKMANAGER_NETWORKINTERFACE_H

#include <solid/control/ifaces/networkinterface.h>

#include <QtCore/qobject.h>
#include <QtDBus/QDBusObjectPath>

class QDBusInterface;

struct NMDBusDeviceProperties {
    QDBusObjectPath path;
    QString interface;
    uint type;
    QString udi;
    bool active;
    uint activationStage;
    QString ipv4Address;
    QString subnetMask;
    QString broadcast;
    QString hardwareAddress;
    QString route;
    QString primaryDNS;
    QString secondaryDNS;
    int mode;
    int strength;
    bool linkActive;
    int speed;
    uint capabilities;
    uint capabilitiesType;
    QString activeNetPath;
    QStringList networks;
};

Q_DECLARE_METATYPE(NMDBusDeviceProperties)

class NMNetworkInterfacePrivate;

class NMNetworkInterface : public QObject, virtual public Solid::Control::Ifaces::NetworkInterface
{
Q_OBJECT
Q_INTERFACES(Solid::Control::Ifaces::NetworkInterface)
public:
    NMNetworkInterface(const QString  & objectPath);
    virtual ~NMNetworkInterface();
    QString uni() const;
    bool isActive() const;
    Solid::Control::NetworkInterface::Type type() const;
    Solid::Control::NetworkInterface::ConnectionState connectionState() const;
    int designSpeed() const;
    Solid::Control::NetworkInterface::Capabilities capabilities() const;
    QString activeNetwork() const;
    // These setters are used to update the interface by the manager
    // in response to DBus signals
    void setActive(bool);
    void setActivationStage(int activationStage);
    void addNetwork(const QDBusObjectPath  & netPath);
    void removeNetwork(const QDBusObjectPath  & netPath);
    void setManagerInterface(QDBusInterface * manager);
    QString interfaceName() const;
    QString driver() const;
    Solid::Control::IPv4Config ipV4Config() const;
    virtual bool activateConnection(const QString & connectionUni, const QVariantMap & connectionParameters);
    virtual bool deactivateConnection();
Q_SIGNALS:
    void ipDetailsChanged();
    void connectionStateChanged(int state);
    void connectionStateChanged(int new_state, int old_state, int reason);
protected:
    NMNetworkInterface(NMNetworkInterfacePrivate &dd);
    NMNetworkInterfacePrivate * d_ptr;
private:
    Q_DECLARE_PRIVATE(NMNetworkInterface)
    Q_DISABLE_COPY(NMNetworkInterface)
};

#endif
