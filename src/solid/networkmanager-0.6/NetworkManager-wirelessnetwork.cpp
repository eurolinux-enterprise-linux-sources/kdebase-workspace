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

#include "NetworkManager-wirelessnetwork.h"

#include "NetworkManager-networkinterface_p.h"

// Copied from wireless.h
/* Modes of operation */
#define IW_MODE_AUTO    0   /* Let the driver decides */
#define IW_MODE_ADHOC   1   /* Single cell network */
#define IW_MODE_INFRA   2   /* Multi cell network, roaming, ... */
#define IW_MODE_MASTER  3   /* Synchronization master or Access Point */
#define IW_MODE_REPEAT  4   /* Wireless Repeater (forwarder) */
#define IW_MODE_SECOND  5   /* Secondary master/repeater (backup) */
#define IW_MODE_MONITOR 6   /* Passive monitor (listen only) */

#include <kdebug.h>

#include "NetworkManager-wirelessaccesspoint.h"

#include <QtDBus/QDBusReply>

void dump(const Solid::Control::WirelessNetworkInterface::Capabilities  & cap)
{
    kDebug(1441) << "WPA      " << (cap  & Solid::Control::WirelessNetworkInterface::Wpa ? "X " : " O");
    kDebug(1441) << "Wep40    " << (cap  & Solid::Control::WirelessNetworkInterface::Wep40 ? "X " : " O");
    kDebug(1441) << "Wep104   " << (cap  & Solid::Control::WirelessNetworkInterface::Wep104 ? "X " : " O");
    kDebug(1441) << "TKIP     " << (cap  & Solid::Control::WirelessNetworkInterface::Tkip ? "X " : " O");
    kDebug(1441) << "CCMP     " << (cap  & Solid::Control::WirelessNetworkInterface::Ccmp ? "X " : " O");
    kDebug(1441) << "RSM      " << (cap  & Solid::Control::WirelessNetworkInterface::Rsn ? "X " : " O");
}

Solid::Control::WirelessNetworkInterface::Capabilities getCapabilities(const int nm)
{
    Solid::Control::WirelessNetworkInterface::Capabilities caps;
    if (nm  & NM_802_11_CAP_NONE)
        caps |= Solid::Control::WirelessNetworkInterface::NoCapability;
#if 0
    if (nm  & NM_802_11_CAP_PROTO_WEP)
        caps |= Solid::Control::WirelessNetworkInterface::Wep;
#endif
    if (nm  & NM_802_11_CAP_PROTO_WPA)
        caps |= Solid::Control::WirelessNetworkInterface::Wpa;
#if 0
    if (nm  & NM_802_11_CAP_PROTO_WPA2)
        caps |= Solid::Control::WirelessNetworkInterface::Wpa2;
    if (nm  & NM_802_11_CAP_KEY_MGMT_PSK)
        caps |= Solid::Control::WirelessNetworkInterface::Psk;
    if (nm  & NM_802_11_CAP_KEY_MGMT_802_1X)
        caps |= Solid::Control::WirelessNetworkInterface::Ieee8021x;
#endif
    if (nm  & NM_802_11_CAP_CIPHER_WEP40)
        caps |= Solid::Control::WirelessNetworkInterface::Wep40;
    if (nm  & NM_802_11_CAP_CIPHER_WEP104)
        caps |= Solid::Control::WirelessNetworkInterface::Wep104;
    if (nm  & NM_802_11_CAP_CIPHER_TKIP)
        caps |= Solid::Control::WirelessNetworkInterface::Tkip;
    if (nm  & NM_802_11_CAP_CIPHER_CCMP)
        caps |= Solid::Control::WirelessNetworkInterface::Ccmp;
    return caps;
}

Solid::Control::WirelessNetworkInterface::OperationMode getOperationMode(const int nm)
{
    Solid::Control::WirelessNetworkInterface::OperationMode mode = Solid::Control::WirelessNetworkInterface::Unassociated;
    switch (nm)
    {
    case IW_MODE_ADHOC:
        mode = Solid::Control::WirelessNetworkInterface::Adhoc;
        break;
    case IW_MODE_INFRA:
    case IW_MODE_MASTER:
        mode = Solid::Control::WirelessNetworkInterface::Managed;
        break;
    case IW_MODE_REPEAT:
        mode = Solid::Control::WirelessNetworkInterface::Repeater;
        break;
    }
    return mode;
}


class NMWirelessNetworkPrivate : public NMNetworkInterfacePrivate
{
public:
    NMWirelessNetworkPrivate(const QString  & netPath)
        : NMNetworkInterfacePrivate(netPath),
        rate(0) { }
    Q_DECLARE_PUBLIC(NMWirelessNetwork)
    /* reimp */ void notifyNewNetwork(const QDBusObjectPath & netPath);
    /* reimp */ void notifyRemoveNetwork(const QDBusObjectPath & netPath);
    /* reimp */ void applyProperties(const NMDBusDeviceProperties & props);
    void readActiveAccessPoint();
    MacAddress hwAddr;
    int rate;
    Solid::Control::WirelessNetworkInterface::OperationMode mode;
    Solid::Control::WirelessNetworkInterface::Capabilities wirelessCapabilities;
    QHash<QString, NMAccessPoint*> accessPoints;
    QString activeAccessPoint;
};

void NMWirelessNetworkPrivate::notifyNewNetwork(const QDBusObjectPath & netPath)
{
    Q_Q(NMWirelessNetwork);
    const QString path = netPath.path();
    QHash<QString, NMAccessPoint*>::ConstIterator it = accessPoints.find(path);
    if (it == accessPoints.end()) {
        accessPoints.insert(path, 0);
        emit q->accessPointAppeared(path);
    }
}

void NMWirelessNetworkPrivate::notifyRemoveNetwork(const QDBusObjectPath & netPath)
{
    Q_Q(NMWirelessNetwork);
    const QString path = netPath.path();
    QHash<QString, NMAccessPoint*>::Iterator it = accessPoints.find(path);
    if (it != accessPoints.end()) {
        NMAccessPoint * ap = it.value();
        accessPoints.erase(it);
        if (path == activeAccessPoint) {
            readActiveAccessPoint();
        }
        emit q->accessPointDisappeared(path);
        delete ap;
    }
}

void NMWirelessNetworkPrivate::readActiveAccessPoint()
{
    activeAccessPoint = QString();
    const QDBusReply<QString> reply = iface.call("getActiveNetwork");
    if (reply.isValid()) {
        activeAccessPoint = reply.value();
    }
}


NMWirelessNetwork::NMWirelessNetwork(const QString  & networkPath)
 : NMNetworkInterface(*new NMWirelessNetworkPrivate(networkPath))
{
    Q_D(NMWirelessNetwork);
    d->readActiveAccessPoint();
}

NMWirelessNetwork::~NMWirelessNetwork()
{
    Q_D(NMWirelessNetwork);
    qDeleteAll(d->accessPoints);
}

void NMWirelessNetworkPrivate::applyProperties(const NMDBusDeviceProperties & props)
{
    NMNetworkInterfacePrivate::applyProperties(props);

    hwAddr = props.hardwareAddress;
    Q_FOREACH (const QString & netudi, props.networks) {
        accessPoints.insert(netudi, 0);
    }
    mode = getOperationMode(props.mode);
    wirelessCapabilities = getCapabilities(props.capabilitiesType);
}

int NMWirelessNetwork::bitRate() const
{
    Q_D(const NMWirelessNetwork);
    return d->rate;
}

Solid::Control::WirelessNetworkInterface::Capabilities NMWirelessNetwork::wirelessCapabilities() const
{
    Q_D(const NMWirelessNetwork);
    return d->wirelessCapabilities;
}

Solid::Control::WirelessNetworkInterface::OperationMode NMWirelessNetwork::mode() const
{
    Q_D(const NMWirelessNetwork);
    return d->mode;
}

void NMWirelessNetwork::setSignalStrength(const QDBusObjectPath & netPath, int strength)
{
    Q_D(NMWirelessNetwork);
    QHash<QString, NMAccessPoint*>::ConstIterator it = d->accessPoints.find(netPath.path());
    if (it != d->accessPoints.end() && it.value()) {
        NMAccessPoint * ap = it.value();
        ap->setSignalStrength(strength);
    }
}

void NMWirelessNetwork::setBitrate(int rate)
{
    Q_D(NMWirelessNetwork);
    if (d->rate == rate)
        return;

    d->rate = rate;
    emit bitRateChanged(rate);
}

#if 0
void NMWirelessNetwork::setActivated(bool activated)
{
    QDBusInterface manager(NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE,
            QDBusConnection::systemBus());
    QString devicePath = uni().left(uni().indexOf("/Networks"));
    kDebug(1441) << devicePath << " - " << d->essid;
    QDBusObjectPath op(devicePath);
#ifdef __GNUC__
#warning fixme hardcoded false fallback bool in setActiveDevice - fixed, i think
#endif
    QList<QVariant> args;
    args << qVariantFromValue(op) << d->essid;// << false;
    bool error;
    args = NMDBusHelper::serialize(d->authentication, d->essid, args, &error);
    kDebug(1441) << " " << args;
    if (error)
        kDebug(1411) << "Error whilst serializing authentication.";
    else
        manager.callWithArgumentList(QDBus::Block, "setActiveDevice", args);
    if (manager.lastError().isValid())
        kDebug(1441) << "setActiveDevice returned error: " << manager.lastError().name() << ": " << manager.lastError().message();

    emit activationStateChanged(activated);
}
#endif

MacAddressList NMWirelessNetwork::accessPoints() const
{
    Q_D(const NMWirelessNetwork);
    return d->accessPoints.keys();
}

QString NMWirelessNetwork::activeAccessPoint() const
{
    Q_D(const NMWirelessNetwork);
    return d->activeAccessPoint;
}

QString NMWirelessNetwork::hardwareAddress() const
{
    Q_D(const NMWirelessNetwork);
    return d->hwAddr;
}

QObject * NMWirelessNetwork::createAccessPoint(const QString & uni)
{
    Q_D(NMWirelessNetwork);
    kDebug() << uni;
    QHash<QString, NMAccessPoint*>::Iterator it = d->accessPoints.find(uni);
    if (it != d->accessPoints.end()) {
        if (it.value()) {
            return it.value();
        } else {
            NMAccessPoint * ap = new NMAccessPoint(uni);
            it.value() = ap;
            return ap;
        }
    } else {
        kDebug(1441) << "no such AP:" << uni;
    }
    return 0;
}

bool NMWirelessNetwork::activateConnection(const QString & connectionUni, const QVariantMap & connectionParameters)
{
    Q_UNUSED(connectionUni)
    Q_UNUSED(connectionParameters)
    return false;
}

bool NMWirelessNetwork::deactivateConnection()
{
    return false;
}

#include "NetworkManager-wirelessnetwork.moc"
