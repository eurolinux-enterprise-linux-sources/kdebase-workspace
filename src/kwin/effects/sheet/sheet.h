/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SHEET_H
#define KWIN_SHEET_H

#include <kwineffects.h>

namespace KWin
{

class SheetEffect
    : public Effect
    {
    public:
        SheetEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );

        static bool supported();
    private:
        class WindowInfo;
        bool isSheetWindow( EffectWindow* w );
        QHash< const EffectWindow*, WindowInfo > windows;
        float duration;
        int windowCount;
        int screenTime;
    };

class SheetEffect::WindowInfo
    {
    public:
        WindowInfo()
            : deleted( false )
            , added( false )
            , closed( false )
            , parentY( 0 )
            {
            timeLine = new TimeLine();
            }
        bool deleted;
        bool added;
        bool closed;
        TimeLine* timeLine;
        int parentY;
    };

} // namespace

#endif
