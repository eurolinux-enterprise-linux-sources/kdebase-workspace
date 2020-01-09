/*
 * Copyright (C) 2007 Menard Alexis <darktears31@gmail.com>
 *
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/


#ifndef HOTPLUGENGINE_H
#define HOTPLUGENGINE_H

#include <QObject>

#include <Plasma/DataEngine>

#include <Solid/Predicate>

/**
 * This class is connected with solid, filter devices and provide signal with source for applet in Plasma
 */
class HotplugEngine : public Plasma::DataEngine
{
    Q_OBJECT

    public:
        HotplugEngine( QObject* parent, const QVariantList& args);
        ~HotplugEngine();
        void init();

    protected slots :
        void onDeviceAdded(const QString &udi);
        void onDeviceRemoved(const QString &udi);

    private :
        void findPredicates();

        QHash<QString, Solid::Predicate> m_predicates;
};

K_EXPORT_PLASMA_DATAENGINE(hotplug, HotplugEngine)

#endif
