/***************************************************************************
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
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

#ifndef CLOCK_H
#define CLOCK_H

#include <QtCore/QTime>
#include <QtCore/QDate>

#include <Plasma/Applet>
#include <Plasma/DataEngine>
#include <Plasma/Dialog>

#include "ui_clockConfig.h"
#include <plasmaclock/clockapplet.h>

class Clock : public ClockApplet
{
    Q_OBJECT
    public:
        Clock(QObject *parent, const QVariantList &args);
        ~Clock();

        void init();
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);

    public slots:
        void dataUpdated(const QString &name, const Plasma::DataEngine::Data &data);
        void updateColors();

    protected slots:
        void clockConfigAccepted();
        void constraintsEvent(Plasma::Constraints constraints);

    protected:
        void createClockConfigurationInterface(KConfigDialog *parent);
        void changeEngineTimezone(const QString &oldTimezone, const QString &newTimezone);

    private:
        void updateSize();
        bool showTimezone() const;
        QRect preparePainter(QPainter *p, const QRect &rect, const QFont &font, const QString &text, bool singleline = false);
        QRectF normalLayout (int subtitleWidth, int subtitleHeight, const QRect &contentsRect);
        QRectF sideBySideLayout (int subtitleWidth, int subtitleHeight, const QRect &contentsRect);

        QFont m_plainClockFont;
        bool m_useCustomColor;
        QColor m_plainClockColor;
        QRect m_timeRect;
        QRect m_dateRect;

        bool m_showDate;
        bool m_showYear;
        bool m_showDay;
        bool m_showSeconds;
        bool m_showTimezone;
        bool m_dateTimezoneBesides;

        int updateInterval() const;
        Plasma::IntervalAlignment intervalAlignment() const;

        QTime m_time;
        QDate m_date;
        QString m_dateString;
        QVBoxLayout *m_layout;
        QTime m_lastTimeSeen;
        QPixmap m_toolTipIcon;
        /// Designer Config files
        Ui::clockConfig ui;
};

K_EXPORT_PLASMA_APPLET(dig_clock, Clock)

#endif
