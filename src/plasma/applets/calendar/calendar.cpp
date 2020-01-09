/***************************************************************************
 *   Copyright 2008 by Davide Bettio <davide.bettio@kdemail.net>           *
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

#include <QGraphicsLayout>
#include <QPainter>
#include <QTimer>

#include <KDebug>
#include <KIcon>
#include <KSystemTimeZones>

#include <Plasma/Svg>
#include <Plasma/Theme>

#include "calendar.h"


CalendarApplet::CalendarApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_calendarDialog(0),
    m_theme(0),
    m_date(0)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setCacheMode(DeviceCoordinateCache);
}

void CalendarApplet::init()
{
    setPopupIcon("view-pim-calendar");
    updateDate();
}

QGraphicsWidget *CalendarApplet::graphicsWidget()
{
    if (!m_calendarDialog) {
        m_calendarDialog = new Plasma::Calendar(this);
        m_calendarDialog->setPreferredSize(220, 250);
    }

    return m_calendarDialog;
}

CalendarApplet::~CalendarApplet()
{

}

void CalendarApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (!m_calendarDialog) {
        graphicsWidget();
    }

    if ((constraints|Plasma::FormFactorConstraint || constraints|Plasma::SizeConstraint) && 
        layout()->itemAt(0) != m_calendarDialog) {
        paintIcon();
    }
}

void CalendarApplet::paintIcon()
{
    const int iconSize = qMin(size().width(), size().height());

    if (iconSize <= 0) {
        return;
    }

    QPixmap icon(iconSize, iconSize);

    if (!m_theme) {
        m_theme = new Plasma::Svg(this);
        m_theme->setImagePath("calendar/mini-calendar");
        m_theme->setContainsMultipleImages(true);
    }

    icon.fill(Qt::transparent);
    QPainter p(&icon);

    m_theme->paint(&p, icon.rect(), "mini-calendar");

    QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);
    p.setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::ButtonTextColor));
    font.setPixelSize(icon.size().height() / 2);
    p.setFont(font);
    p.drawText(icon.rect().adjusted(0, icon.size().height()/4, 0, 0), Qt::AlignCenter, QString::number(m_date));
    m_theme->resize();
    p.end();
    setPopupIcon(icon);
}

void CalendarApplet::configAccepted()
{
    update();
}

void CalendarApplet::updateDate()
{
    QDateTime d = QDateTime::currentDateTime();
    m_date = d.date().day();
    int updateIn = (24 * 60 * 60) - (d.toTime_t() + KSystemTimeZones::local().currentOffset()) % (24 * 60 * 60);
    QTimer::singleShot(updateIn * 1000, this, SLOT(updateDate()));
    constraintsEvent(Plasma::FormFactorConstraint);
}

#include "calendar.moc"
