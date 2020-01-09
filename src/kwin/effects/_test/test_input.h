/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Testing of handling input in effects.

*/

#ifndef KWIN_TEST_INPUT_H
#define KWIN_TEST_INPUT_H

#include <kwineffects.h>

namespace KWin
{

class TestInputEffect
    : public Effect
    {
    public:
        TestInputEffect();
        virtual ~TestInputEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
    private:
        Window input;
    };

} // namespace

#endif
