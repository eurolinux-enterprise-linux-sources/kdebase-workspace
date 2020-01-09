/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "drunken.h"

#include <math.h>

namespace KWin
{

KWIN_EFFECT( drunken, DrunkenEffect )

void DrunkenEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( !windows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void DrunkenEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ))
        {
        windows[ w ] += time / 1000.;
        if( windows[ w ] < 1 )
            data.setTransformed();
        else
            windows.remove( w );
        }
    effects->prePaintWindow( w, data, time );
    }

void DrunkenEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( !windows.contains( w ))
        {
        effects->paintWindow( w, mask, region, data );
        return;
        }
    WindowPaintData d1 = data;
    // 4 cycles, decreasing amplitude
    int diff = int( sin( windows[ w ] * 8 * M_PI ) * ( 1 - windows[ w ] ) * 10 );
    d1.xTranslate -= diff;
    d1.opacity *= 0.5;
    effects->paintWindow( w, mask, region, d1 );
    WindowPaintData d2 = data;
    d2.xTranslate += diff;
    d2.opacity *= 0.5;
    effects->paintWindow( w, mask, region, d2 );
    }

void DrunkenEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        w->addRepaintFull();
    effects->postPaintWindow( w );
    }

void DrunkenEffect::windowAdded( EffectWindow* w )
    {
    windows[ w ] = 0;
    w->addRepaintFull();
    }

void DrunkenEffect::windowClosed( EffectWindow* w )
    {
    windows.remove( w );
    }

} // namespace
