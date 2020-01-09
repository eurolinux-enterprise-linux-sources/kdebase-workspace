/*
 *   Copyright 2008 Aike J Sommer <dev@aikesommer.name>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef KEPHAL_DBUSCONFIGURATIONS_H
#define KEPHAL_DBUSCONFIGURATIONS_H

#include <QPoint>
#include "kephal/configurations.h"
#include "configurations_interface.h"


namespace Kephal {

    class DBusConfigurations;

    class DBusConfiguration : public Configuration {
        Q_OBJECT
        public:
            DBusConfiguration(DBusConfigurations * parent, QString name);
            
            QString name();
            bool isModifiable();
            bool isActivated();
            QMap<int, QPoint> layout();
            int primaryScreen();
            
        public Q_SLOTS:
            void activate();
            
        private:
            QString m_name;
            DBusConfigurations * m_parent;
    };
    
    
    class DBusConfigurations : public Configurations {
        Q_OBJECT
        public:
            DBusConfigurations(QObject * parent);
            
            QMap<QString, Configuration *> configurations();
            Configuration * activeConfiguration();
            QList<Configuration *> alternateConfigurations();
            QList<QPoint> possiblePositions(Output * output);
            bool move(Output * output, const QPoint & position);
            bool resize(Output * output, const QSize & size);
            bool rotate(Output * output, Rotation rotation);
            bool reflectX(Output * output, bool reflect);
            bool reflectY(Output * output, bool reflect);
            bool changeRate(Output * output, float rate);
            int screen(Output * output);
            void applyOutputSettings();
            void setPolling(bool polling);
            bool polling();
            void confirm();
            void revert();
            
            bool isValid();
            org::kde::Kephal::Configurations * interface();
            
        private Q_SLOTS:
            void configurationActivatedSlot(QString name);
            
        private:
            org::kde::Kephal::Configurations * m_interface;
            bool m_valid;
            QMap<QString, DBusConfiguration *> m_configs;
    };
    
}


#endif // KEPHAL_DBUSCONFIGURATIONS_H

