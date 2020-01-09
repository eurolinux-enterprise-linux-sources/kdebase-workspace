/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2009 Marco Martin <notmart@gmail.com>                   *
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

#ifndef SYSTEMTRAYTYPES_H
#define SYSTEMTRAYTYPES_H

#include <QDBusArgument>
#include <QVector>

struct ExperimentalKDbusImageStruct {
    int width;
    int height;
    QByteArray data;
};

const QDBusArgument &operator<<(QDBusArgument &argument, const ExperimentalKDbusImageStruct &icon);
const QDBusArgument &operator>>(const QDBusArgument &argument, ExperimentalKDbusImageStruct &icon);

Q_DECLARE_METATYPE(ExperimentalKDbusImageStruct)

typedef QVector<ExperimentalKDbusImageStruct> ExperimentalKDbusImageVector;
const QDBusArgument &operator<<(QDBusArgument &argument, const ExperimentalKDbusImageVector &iconVector);
const QDBusArgument &operator>>(const QDBusArgument &argument, ExperimentalKDbusImageVector &iconVector);

Q_DECLARE_METATYPE(ExperimentalKDbusImageVector)


struct ExperimentalKDbusToolTipStruct {
    QString icon;
    ExperimentalKDbusImageVector image;
    QString title;
    QString subTitle;
};

const QDBusArgument &operator<<(QDBusArgument &argument, const ExperimentalKDbusToolTipStruct &toolTip);
const QDBusArgument &operator>>(const QDBusArgument &argument, ExperimentalKDbusToolTipStruct &toolTip);

Q_DECLARE_METATYPE(ExperimentalKDbusToolTipStruct)

#endif
