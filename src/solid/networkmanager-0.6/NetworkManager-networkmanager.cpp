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

#include "NetworkManager-networkmanager.h"

#include <NetworkManager/NetworkManager.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusReply>

#include <kdebug.h>

#include "NetworkManager-wirednetwork.h"
#include "NetworkManager-wirelessnetwork.h"

class NMNetworkManagerPrivate
{
public:
    NMNetworkManagerPrivate();

    void fillNetworkInterfacesList();

    QDBusInterface manager;
    QHash<QString, NMNetworkInterface *> interfaces;
    NMState cachedState;
    bool wirelessEnabled;
    bool wirelessHardwareEnabled;
};

NMNetworkManagerPrivate::NMNetworkManagerPrivate()
    : manager(NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, QDBusConnection::systemBus())
    , cachedState(NM_STATE_UNKNOWN)
    , wirelessEnabled(false)
    , wirelessHardwareEnabled(false)
{
}


NMNetworkManager::NMNetworkManager(QObject * parent, const QVariantList  & /*args */)
 : NetworkManager(parent), d(new NMNetworkManagerPrivate)
{
    #define connectNMToThis(signal, slot) \
    d->manager.connection().connect(NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, \
                                     signal, this, SLOT(slot));
    connectNMToThis(NM_DBUS_SIGNAL_STATE_CHANGE, stateChanged(uint));
    connectNMToThis("DeviceAdded", receivedDeviceAdded(QDBusObjectPath));
    connectNMToThis("DeviceRemoved", receivedDeviceRemoved(QDBusObjectPath));
    connectNMToThis("DeviceStrengthChanged", deviceStrengthChanged(QDBusObjectPath,int));
    connectNMToThis("WirelessNetworkStrengthChanged", networkStrengthChanged(QDBusObjectPath,QDBusObjectPath,int));
    connectNMToThis("WirelessNetworkAppeared", wirelessNetworkAppeared(QDBusObjectPath,QDBusObjectPath));
    connectNMToThis("WirelessNetworkDisappeared", wirelessNetworkDisappeared(QDBusObjectPath,QDBusObjectPath));
    connectNMToThis("DeviceActivationStage", deviceActivationStageChanged(QDBusObjectPath,uint));

    connectNMToThis("DeviceCarrierOn", carrierOn(QDBusObjectPath));
    connectNMToThis("DeviceCarrierOff", carrierOff(QDBusObjectPath));
    connectNMToThis("DeviceNowActive", nowActive(QDBusObjectPath));
    connectNMToThis("DeviceNoLongerActive", noLongerActive(QDBusObjectPath));
    connectNMToThis("DeviceActivating", activating(QDBusObjectPath));
    //TODO: find a way to connect to the wireless variant of this, incl essid
    connectNMToThis("DeviceActivationFailed", activationFailed(QDBusObjectPath));
    connectNMToThis("WirelessEnabled", wirelessEnabled(bool, bool));

    connect(QDBusConnection::systemBus().interface(), SIGNAL(serviceOwnerChanged(QString, QString, QString)),
            this, SLOT(nameOwnerChanged(QString,QString,QString)));

    qDBusRegisterMetaType<QList<QDBusObjectPath> >();

    d->fillNetworkInterfacesList();

    const QDBusMessage wirelessEnabledReply = d->manager.call("getWirelessEnabled");
    if (wirelessEnabledReply.type() == QDBusMessage::ReplyMessage)
    {
        const QList<QVariant> args = wirelessEnabledReply.arguments();
        if (args.size() > 0) d->wirelessEnabled = args[0].toBool();
        if (args.size() > 1) d->wirelessHardwareEnabled = args[1].toBool();
    }
}

NMNetworkManager::~NMNetworkManager()
{
    delete d;
}

Solid::Networking::Status NMNetworkManager::status() const
{
    if (NM_STATE_UNKNOWN == d->cachedState)
    {
        QDBusReply< uint > state = d->manager.call("state");
        if (state.isValid())
        {
            kDebug(1441) << "  got state: " << state.value();
            d->cachedState = static_cast<NMState>(state.value());
        }
    }
    switch ( d->cachedState ) {
        case NM_STATE_CONNECTING:
            return Solid::Networking::Connecting;
            break;
        case NM_STATE_CONNECTED:
            return Solid::Networking::Connected;
            break;
        case NM_STATE_DISCONNECTED:
            return Solid::Networking::Unconnected;
            break;
        default:
        case NM_STATE_UNKNOWN:
        case NM_STATE_ASLEEP:
            return Solid::Networking::Unknown;
            break;
    }
}

QStringList NMNetworkManager::networkInterfaces() const
{
    kDebug(1441);
    return d->interfaces.keys();
}

void NMNetworkManagerPrivate::fillNetworkInterfacesList()
{
    // wtf does this work when not called on org.freedesktop.NetworkManager.Devices?
    QDBusReply< QList <QDBusObjectPath> > deviceList = manager.call("getDevices");
    if (deviceList.isValid())
    {
        kDebug(1441) << "Got device list";
        QList <QDBusObjectPath> devices = deviceList.value();
        foreach (const QDBusObjectPath & op, devices)
        {
            QHash<QString, NMNetworkInterface *>::ConstIterator it = interfaces.find(op.path());
            if (it == interfaces.end())
            {
                interfaces.insert(op.path(), 0);
                kDebug(1441) << "  adding:" << op.path();
            }
        }
    }
    else
        kDebug(1441) << "Error getting device list: " << deviceList.error().name() << ": " << deviceList.error().message();
}

QObject * NMNetworkManager::createNetworkInterface(const QString  & uni)
{
    kDebug(1441) << uni;
    NMNetworkInterface * netInterface = 0;
    QHash<QString, NMNetworkInterface *>::Iterator it = d->interfaces.find(uni);
    if (it == d->interfaces.end())
    {
        kDebug(1441) << "unknown interface:" << uni;
        return 0;
    }
    if (it.value())
    {
        netInterface = it.value();
    }
    else
    {
        QDBusInterface iface(NM_DBUS_SERVICE,
                             uni,
                             NM_DBUS_INTERFACE_DEVICES,
                             QDBusConnection::systemBus());
        QDBusReply<int> reply = iface.call("getType");
        if (!reply.isValid())
        {
            kDebug(1441) << "Invalid reply, most probably the specified device does not exists.";
            return 0;
        }
        const int type = reply.value();
        switch (type)
        {
        case DEVICE_TYPE_802_3_ETHERNET:
            netInterface = new NMWiredNetwork(uni);
            break;
        case DEVICE_TYPE_802_11_WIRELESS:
            netInterface = new NMWirelessNetwork(uni);
            break;
        case DEVICE_TYPE_UNKNOWN:
        default:
            ;
        }

        if (netInterface)
        {
            netInterface->setManagerInterface(&d->manager);
            it.value() = netInterface;
        }
    }
    return netInterface;
}

bool NMNetworkManager::isNetworkingEnabled() const
{
    kDebug(1441);
    if (NM_STATE_UNKNOWN == d->cachedState)
    {
        QDBusReply< uint > state = d->manager.call("state");
        if (state.isValid())
        {
            kDebug(1441) << "  got state: " << state.value();
            d->cachedState = static_cast<NMState>(state.value());
        }
    }
    return NM_STATE_CONNECTING == d->cachedState || NM_STATE_CONNECTED == d->cachedState || NM_STATE_DISCONNECTED == d->cachedState;
}

bool NMNetworkManager::isWirelessEnabled() const
{
    kDebug(1441);
    return d->wirelessEnabled;
}

bool NMNetworkManager::isWirelessHardwareEnabled() const
{
    kDebug(1441);
    return d->wirelessHardwareEnabled;
}

QStringList NMNetworkManager::activeConnections() const
{
    kDebug(1441);
    QStringList activeConnections;
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.constBegin(), itEnd = d->interfaces.constEnd();
    for ( ; it != itEnd; ++it)
    {
        NMNetworkInterface * interface = it.value();
        if (interface && interface->isActive())
        {
            activeConnections << it.key();
        }
    }
    return activeConnections;
}

Solid::Control::NetworkInterface::Types NMNetworkManager::supportedInterfaceTypes() const
{
    return (Solid::Control::NetworkInterface::Types) (
           Solid::Control::NetworkInterface::Ieee80211 |
           Solid::Control::NetworkInterface::Ieee8023
           );
}

void NMNetworkManager::activateConnection(const QString & interfaceUni, const QString & connectionUni, const QVariantMap & connectionParameters)
{
    kDebug(1441) << interfaceUni << connectionUni << connectionParameters;
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(interfaceUni);
    if (it != d->interfaces.end())
    {
        NMNetworkInterface * interface = it.value();
        if (!interface)
            interface = qobject_cast<NMNetworkInterface *>(createNetworkInterface(interfaceUni));
        if (interface)
        {
            bool activated = interface->activateConnection(connectionUni, connectionParameters);
            Q_UNUSED(activated)
        }
    }
}

void NMNetworkManager::deactivateConnection(const QString & activeConnection)
{
    kDebug(1441) << activeConnection;
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(activeConnection);
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        bool deactivated = interface->deactivateConnection();
        Q_UNUSED(deactivated)
    }
}

void NMNetworkManager::setNetworkingEnabled(bool enabled)
{
    kDebug(1441) << enabled;
    d->manager.call(enabled ? "wake" : "sleep"); //TODO Find out the semantics of the optional bool argument to 'sleep'
}

void NMNetworkManager::setWirelessEnabled(bool enabled)
{
    kDebug(1441) << enabled;
    d->manager.call("setWirelessEnabled", enabled);
}

void NMNetworkManager::stateChanged(uint state)
{
    d->cachedState = static_cast<NMState>(state);
    switch ( d->cachedState ) {
        case NM_STATE_CONNECTING:
            kDebug(1441) << "Connecting";
            emit statusChanged( Solid::Networking::Connecting );
            break;
        case NM_STATE_CONNECTED:
            kDebug(1441) << "CONNECTED";
            emit statusChanged( Solid::Networking::Connected );
            break;
        case NM_STATE_ASLEEP:
        case NM_STATE_DISCONNECTED:
            kDebug(1441) << "Unconnected";
            emit statusChanged( Solid::Networking::Unconnected );
            break;
        default:
        case NM_STATE_UNKNOWN:
            kDebug(1441) << "Unknown";
            emit statusChanged( Solid::Networking::Unknown );
            break;
    }
}

void NMNetworkManager::receivedDeviceAdded(const QDBusObjectPath & objpath)
{
    kDebug(1441) << objpath.path();
    const QString path = objpath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(path);
    if (it == d->interfaces.end())
    {
        d->interfaces.insert(path, 0);
        emit networkInterfaceAdded(path);
    }
}

void NMNetworkManager::receivedDeviceRemoved(const QDBusObjectPath & objpath)
{
    kDebug(1441) << objpath.path();
    const QString path = objpath.path();
    QHash<QString, NMNetworkInterface *>::Iterator it = d->interfaces.find(path);
    if (it != d->interfaces.end())
    {
        NMNetworkInterface * iface = it.value();
        d->interfaces.erase(it);
        emit networkInterfaceRemoved(path);
        delete iface;
    }
}

void NMNetworkManager::deviceStrengthChanged(const QDBusObjectPath & devPath, int strength)
{
    kDebug(1441) << devPath.path() << strength;
}

void NMNetworkManager::networkStrengthChanged(const QDBusObjectPath & devPath, const QDBusObjectPath & netPath, int strength)
{
    kDebug(1441) << devPath.path() << "," << netPath.path() << "," << strength;
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        NMWirelessNetwork * wifiNet = qobject_cast<NMWirelessNetwork *>(interface);
        if (wifiNet)
            wifiNet->setSignalStrength(netPath, strength);
    }
}

void NMNetworkManager::wirelessNetworkAppeared(const QDBusObjectPath & devPath, const QDBusObjectPath & netPath)
{
    kDebug(1441) << devPath.path() << "," << netPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->addNetwork(netPath);
    }
}

void NMNetworkManager::wirelessNetworkDisappeared(const QDBusObjectPath & devPath, const QDBusObjectPath & netPath)
{
    kDebug(1441) << devPath.path() << "," << netPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->removeNetwork(netPath);
    }
}

void NMNetworkManager::deviceActivationStageChanged(const QDBusObjectPath & devPath, uint stage)
{
    kDebug(1441) << devPath.path() << "("<< stage << ")";
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->setActivationStage(stage);
    }
}

void NMNetworkManager::carrierOn(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        NMWiredNetwork * wiredNet = qobject_cast<NMWiredNetwork *>(interface);
        if (wiredNet)
            wiredNet->setCarrier(true);
    }
}

void NMNetworkManager::carrierOff(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        NMWiredNetwork * wiredNet = qobject_cast<NMWiredNetwork *>(interface);
        if (wiredNet)
            wiredNet->setCarrier(false);
    }
}

void NMNetworkManager::nowActive(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->setActive(true);
    }
}

void NMNetworkManager::noLongerActive(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->setActive(false);
    }
}

void NMNetworkManager::activating(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    // We don't do anything with this signal as it is duplicated by connectionStateChanged
}

void NMNetworkManager::activationFailed(const QDBusObjectPath & devPath)
{
    kDebug(1441) << devPath.path();
    QHash<QString, NMNetworkInterface *>::ConstIterator it = d->interfaces.find(devPath.path());
    if (it != d->interfaces.end() && it.value())
    {
        NMNetworkInterface * interface = it.value();
        interface->setActivationStage(NM_ACT_STAGE_FAILED);
    }
}

void NMNetworkManager::wirelessEnabled(bool wirelessEnabled, bool wirelessHardwareEnabled)
{
    kDebug(1441) << wirelessEnabled << wirelessHardwareEnabled;
    if (wirelessEnabled != d->wirelessEnabled)
    {
        d->wirelessEnabled = wirelessEnabled;
        emit wirelessEnabledChanged(d->wirelessEnabled);
    }
    if (wirelessHardwareEnabled != d->wirelessHardwareEnabled)
    {
        d->wirelessHardwareEnabled = wirelessHardwareEnabled;
        emit wirelessHardwareEnabledChanged(d->wirelessHardwareEnabled);
    }
}

void NMNetworkManager::nameOwnerChanged(const QString & name, const QString & oldOwner, const QString & newOwner)
{
    if (name == QLatin1String(NM_DBUS_SERVICE))
    {
        kDebug(1441) << "name: " << name << ", old owner: " << oldOwner << ", new owner: " << newOwner;
        if ( oldOwner.isEmpty() && !newOwner.isEmpty() ) {
            // NetworkManager started, but we are already listening to StateChanged so we should get
            // its status that way
            ;
        }
        if ( !oldOwner.isEmpty() && newOwner.isEmpty() ) {
            // NetworkManager stopped, set status Unknown for safety
            stateChanged(NM_STATE_UNKNOWN);
        }
    }
}

#include "NetworkManager-networkmanager.moc"
