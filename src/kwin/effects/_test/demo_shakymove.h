/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef DEMO_SHAKYMOVE_H
#define DEMO_SHAKYMOVE_H

#include <qtimer.h>

#include <kwineffects.h>

namespace KWin
{

class ShakyMoveEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShakyMoveEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowClosed( EffectWindow* c );
    private slots:
        void tick();
    private:
        QHash< const EffectWindow*, int > windows;
        QTimer timer;
    };

} // namespace

#endif // DEMO_SHAKYMOVE_H
