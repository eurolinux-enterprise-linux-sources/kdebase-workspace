/*
 * This file was generated by dbusxml2cpp version 0.6
 * Command line was: dbusxml2cpp -N -m -p nm-vpn-connectioninterface /space/kde/sources/trunk/KDE/kdebase/workspace/solid/networkmanager-0.7/dbus/introspection/nm-vpn-connection.xml
 *
 * dbusxml2cpp is Copyright (C) 2006 Trolltech ASA. All rights reserved.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef NM_VPN_CONNECTIONINTERFACE_H_1205050594
#define NM_VPN_CONNECTIONINTERFACE_H_1205050594

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.freedesktop.NetworkManager.VPN.Connection
 */
class OrgFreedesktopNetworkManagerVPNConnectionInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.NetworkManager.VPN.Connection"; }

public:
    OrgFreedesktopNetworkManagerVPNConnectionInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgFreedesktopNetworkManagerVPNConnectionInterface();

    Q_PROPERTY(QString Banner READ banner)
    inline QString banner() const
    { return qvariant_cast< QString >(internalPropGet("Banner")); }

    Q_PROPERTY(QString Name READ name)
    inline QString name() const
    { return qvariant_cast< QString >(internalPropGet("Name")); }

    Q_PROPERTY(uint State READ state)
    inline uint state() const
    { return qvariant_cast< uint >(internalPropGet("State")); }

public Q_SLOTS: // METHODS
    inline QDBusReply<void> Disconnect()
    {
        QList<QVariant> argumentList;
        return callWithArgumentList(QDBus::Block, QLatin1String("Disconnect"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void StateChanged(uint state, uint reason);
};

#endif
