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
 ***************************************************************************/

#ifndef POWERDEVILRUNNER_H
#define POWERDEVILRUNNER_H

#include <Plasma/AbstractRunner>
#include <QDBusConnection>

class PowerDevilRunner : public Plasma::AbstractRunner
{
        Q_OBJECT

    public:
        PowerDevilRunner( QObject *parent, const QVariantList &args );
        ~PowerDevilRunner();

        void match( Plasma::RunnerContext &context );
        void run( const Plasma::RunnerContext &context, const Plasma::QueryMatch &action );

    private slots:
        void updateStatus();

    private:
        void initUpdateTriggers();

        QDBusConnection m_dbus;

        QStringList m_supportedGovernors;
        QHash<QString, int> m_governorData;
        QStringList m_availableProfiles;
        QHash<QString, QString> m_profileIcon;
        QStringList m_supportedSchemes;
        QStringList m_suspendMethods;
        QHash<QString, int> m_suspendData;

        int m_shortestCommand;
};

K_EXPORT_PLASMA_RUNNER( powerdevil, PowerDevilRunner )

#endif
