#include <kdebug.h>
#include <QApplication>
#include <QObject>

#include "bluez-bluetoothmanager.h"
#include "bluez-bluetoothinterface.h"
//#include "bluez-bluetoothremotedevice.h"

int main(int argc, char **argv)
{

    QApplication app(argc, argv);
    BluezBluetoothManager mgr(0, QStringList());

    kDebug() << "Interfaces: " << mgr.bluetoothInterfaces();
    kDebug() << "Default Interface: " << mgr.defaultInterface();
    kDebug() << "Finding Interface hci0: " << mgr.findInterface("hci0");


//  kDebug() << "Bluetooth Input Devices: " << mgr.bluetoothInputDevices();

    BluezBluetoothInterface iface(mgr.defaultInterface());

    iface.setProperty("Name","testNAME");

    QMap<QString, QVariant> props =  iface.getProperties();

    foreach(const QString &key, props.keys()) {
        kDebug() << "Interface Property: " << key << " : " << props[key];
    }


    iface.startDiscovery();

#if 0
    BluezBluetoothRemoteDevice remote("/org/bluez/hci0/00:16:BC:15:A3:FF");

    kDebug() << "Name: " << remote.name();
    kDebug() << "Company: " << remote.company();
    kDebug() << "Services: " << remote.serviceClasses();
    kDebug() << "Major Class: " << remote.majorClass();
    kDebug() << "Minor Class: " << remote.minorClass();

    if (remote.hasBonding()) {
        remote.removeBonding();
    }

    remote.createBonding();


    kDebug() << mgr.setupInputDevice("/org/bluez/hci0/00:04:61:81:75:FF");
#endif

    return app.exec();
}
