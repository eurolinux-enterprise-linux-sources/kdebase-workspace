/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "minimizeanimation.h"

namespace KWin
{

KWIN_EFFECT( minimizeanimation, MinimizeAnimationEffect )

MinimizeAnimationEffect::MinimizeAnimationEffect()
    {
    mActiveAnimations = 0;
    }


void MinimizeAnimationEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    mActiveAnimations = mTimeLineWindows.count();
    if( mActiveAnimations > 0 )
        // We need to mark the screen windows as transformed. Otherwise the
        //  whole screen won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void MinimizeAnimationEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mTimeLineWindows.contains( w ))
        {
        if( w->isMinimized() )
            {
            mTimeLineWindows[w].addTime(time);
            if( mTimeLineWindows[w].progress() >= 1.0f )
                mTimeLineWindows.remove( w );
            }
        else
            {
            mTimeLineWindows[w].removeTime(time);
            if( mTimeLineWindows[w].progress() <= 0.0f )
                mTimeLineWindows.remove( w );
            }

        // Schedule window for transformation if the animation is still in
        //  progress
        if( mTimeLineWindows.contains( w ))
            {
            // We'll transform this window
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            }
        }

    effects->prePaintWindow( w, data, time );
    }

void MinimizeAnimationEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mTimeLineWindows.contains( w ))
    {
        // 0 = not minimized, 1 = fully minimized
        double progress = mTimeLineWindows[w].value();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if( !icon.isValid() )
            icon = QRect( displayWidth() / 2, displayHeight() / 2, 0, 0 );

        data.xScale *= interpolate(1.0, icon.width() / (double)geo.width(), progress);
        data.yScale *= interpolate(1.0, icon.height() / (double)geo.height(), progress);
        data.xTranslate = (int)interpolate(data.xTranslate, icon.x() - geo.x(), progress);
        data.yTranslate = (int)interpolate(data.yTranslate, icon.y() - geo.y(), progress);
        data.opacity *= 0.1 + (1-progress)*0.9;
    }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void MinimizeAnimationEffect::postPaintScreen()
    {
    if( mActiveAnimations > 0 )
        // Repaint the workspace so that everything would be repainted next time
        effects->addRepaintFull();
    mActiveAnimations = mTimeLineWindows.count();

    // Call the next effect.
    effects->postPaintScreen();
    }

void MinimizeAnimationEffect::windowMinimized( EffectWindow* w )
    {
    mTimeLineWindows[w].setCurveShape(TimeLine::EaseInCurve);
    mTimeLineWindows[w].setDuration( animationTime( 250 ));
    mTimeLineWindows[w].setProgress(0.0f);
    }

void MinimizeAnimationEffect::windowUnminimized( EffectWindow* w )
    {
    mTimeLineWindows[w].setCurveShape(TimeLine::EaseOutCurve);
    mTimeLineWindows[w].setDuration( animationTime( 250 ));
    mTimeLineWindows[w].setProgress(1.0f);
    }

} // namespace

