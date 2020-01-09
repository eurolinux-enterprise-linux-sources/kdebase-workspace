/***************************************************************************
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

#ifndef BATTERY_H
#define BATTERY_H

#include <QLabel>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsGridLayout>
#include <QPair>
#include <QMap>

#include <Plasma/Applet>
#include <Plasma/Animator>
#include <Plasma/DataEngine>
#include <Plasma/PopupApplet>
#include "ui_batteryConfig.h"

namespace Plasma
{
    class Svg;
    class Label;
    class ExtenderItem;
    class ComboBox;
    class Slider;
}

class Battery : public Plasma::PopupApplet
{
    Q_OBJECT
    public:
        Battery(QObject *parent, const QVariantList &args);
        ~Battery();

        void init();
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            const QRect &contents);
        void setPath(const QString&);
        Qt::Orientations expandingDirections() const;
        void constraintsEvent(Plasma::Constraints constraints);
        void popupEvent(bool show);
        void showBatteryLabel(bool show);

    public slots:
        void dataUpdated(const QString &name, const Plasma::DataEngine::Data &data);

    protected Q_SLOTS:
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
        void configAccepted();
        void readColors();

    protected:
        void createConfigurationInterface(KConfigDialog *parent);
        void setEmbedded(const bool embedded);

    private slots:
        void animationUpdate(qreal progress);
        void acAnimationUpdate(qreal progress);
        void batteryAnimationUpdate(qreal progress);
        void sourceAdded(const QString &source);
        void sourceRemoved(const QString &source);
        void brightnessChanged(const int brightness);
        void updateSlider(const float brightness);
        void setFullBrightness();
        void openConfig();
        void setProfile(const QString &profile);
        void suspend();
        void hibernate();

    private:
        void connectSources();
        void initExtenderItem(Plasma::ExtenderItem *item);
        void updateStatus();

        /* Prevent creating infinite loops by embedding applets inside applets */
        bool m_isEmbedded;
        Battery *m_extenderApplet;
        bool m_extenderVisible;

        QGraphicsGridLayout *m_controlsLayout;
        QGraphicsGridLayout *m_batteryLayout;
        QGraphicsLinearLayout *m_brightnessLayout;
        Plasma::Label *m_statusLabel;
        Plasma::Label *m_batteryLabel;
        Plasma::Label *m_profileLabel;
        Plasma::ComboBox *m_profileCombo;
        Plasma::Slider *m_brightnessSlider;
        int m_inhibitCookie;

        /* Paint battery with proper charge level */
        void paintBattery(QPainter *p, const QRect &contentsRect, const int batteryPercent, const bool plugState);
        /* Paint a label on top of the battery */
        void paintLabel(QPainter *p, const QRect &contentsRect, const QString& labelText);
        /* Scale in/out Battery. */
        void showBattery(bool show);
        /* Scale in/out Ac Adapter. */
        void showAcAdapter(bool show);
        /* Fade in/out the label above the battery. */
        void showLabel(bool show);
        /* Scale in a QRectF */
        QRectF scaleRectF(qreal progress, QRectF rect);
        /* Show multiple batteries with individual icons and charge info? */
        bool m_showMultipleBatteries;
        /* Should the battery charge information be shown on top? */
        bool m_showBatteryString;
        /* Should that info be percentage (false) or time (true)? */
        bool m_showRemainingTime;
        int m_minutes;
        int m_hours;
        QSizeF m_size;
        int m_pixelSize;
        Plasma::Svg* m_theme;

        QStringList m_availableProfiles;
        QString m_currentProfile;
        QStringList m_suspendMethods;

        // Configuration dialog
        Ui::batteryConfig ui;

        int m_animId;
        qreal m_alpha;
        bool m_fadeIn;

        int m_acAnimId;
        qreal m_acAlpha;
        bool m_acFadeIn;

        int m_batteryAnimId;
        qreal m_batteryAlpha;
        bool m_batteryFadeIn;

        // Internal data
        QList<QVariant> batterylist, acadapterlist;
        QHash<QString, QHash<QString, QVariant> > m_batteries_data;
        QFont m_font;
        bool m_isHovered;
        bool m_firstRun;
        QColor m_boxColor;
        QColor m_textColor;
        QRectF m_textRect;
        int m_boxAlpha;
        int m_boxHoverAlpha;
        int m_numOfBattery;
        bool m_acAdapterPlugged;
        int m_remainingMSecs;
};

K_EXPORT_PLASMA_APPLET(battery, Battery)

#endif
