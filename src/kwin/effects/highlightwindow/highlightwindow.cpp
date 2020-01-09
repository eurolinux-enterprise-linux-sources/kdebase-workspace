/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "highlightwindow.h"

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( highlightwindow, HighlightWindowEffect )

HighlightWindowEffect::HighlightWindowEffect()
    : m_finishing( false )
    , m_fadeDuration( double( animationTime( 150 )))
    , m_monitorWindow( NULL )
    {
    m_atom = XInternAtom( display(), "_KDE_WINDOW_HIGHLIGHT", False );
    effects->registerPropertyType( m_atom, true );

    // Announce support by creating a dummy version on the root window
    unsigned char dummy = 0;
    XChangeProperty( display(), rootWindow(), m_atom, m_atom, 8, PropModeReplace, &dummy, 1 );
    }

HighlightWindowEffect::~HighlightWindowEffect()
    {
    XDeleteProperty( display(), rootWindow(), m_atom );
    effects->registerPropertyType( m_atom, false );
    }

void HighlightWindowEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // Calculate window opacities
    if( !m_highlightedWindows.isEmpty() )
        { // Initial fade out and changing highlight animation
        double oldOpacity = m_windowOpacity[w];
        if( m_highlightedWindows.contains( w ) )
            m_windowOpacity[w] = qMin( 1.0, m_windowOpacity[w] + time / m_fadeDuration );
        else if( w->isNormalWindow() || w->isDialog() ) // Only fade out windows
            m_windowOpacity[w] = qMax( 0.15, m_windowOpacity[w] - time / m_fadeDuration );

        if( m_windowOpacity[w] != 1.0 )
            data.setTranslucent();
        if( oldOpacity != m_windowOpacity[w] )
            w->addRepaintFull();
        }
    else if( m_finishing && m_windowOpacity.contains( w ))
        { // Final fading back in animation
        double oldOpacity = m_windowOpacity[w];
        m_windowOpacity[w] = qMin( 1.0, m_windowOpacity[w] + time / m_fadeDuration );

        if( m_windowOpacity[w] != 1.0 )
            data.setTranslucent();
        if( oldOpacity != m_windowOpacity[w] )
            w->addRepaintFull();

        if( m_windowOpacity[w] == 1.0 )
            m_windowOpacity.remove( w ); // We default to 1.0
        }

    effects->prePaintWindow( w, data, time );
    }

void HighlightWindowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( m_windowOpacity.contains( w ))
        data.opacity *= m_windowOpacity[w];
    effects->paintWindow( w, mask, region, data );
    }

void HighlightWindowEffect::windowAdded( EffectWindow* w )
    {
    if( !m_highlightedWindows.isEmpty() )
        { // The effect is activated thus we need to add it to the opacity hash
        if( w->isNormalWindow() || w->isDialog() ) // Only fade out windows
            m_windowOpacity[w] = 0.15;
        else
            m_windowOpacity[w] = 1.0;
        }
    propertyNotify( w, m_atom ); // Check initial value
    }

void HighlightWindowEffect::windowClosed( EffectWindow* w )
    {
    if( m_monitorWindow == w ) // The monitoring window was destroyed
        finishHighlighting();
    }

void HighlightWindowEffect::windowDeleted( EffectWindow* w )
    {
    m_windowOpacity.remove( w );
    }

void HighlightWindowEffect::propertyNotify( EffectWindow* w, long a )
    {
    if( a != m_atom )
        return; // Not our atom

    QByteArray byteData = w->readProperty( m_atom, m_atom, 32 );
    if( byteData.length() < 1 )
        { // Property was removed, clearing highlight
        finishHighlighting();
        return;
        }
    long* data = reinterpret_cast<long*>( byteData.data() );

    if( !data[0] )
        { // Purposely clearing highlight by issuing a NULL target
        finishHighlighting();
        return;
        }
    m_monitorWindow = w;
    bool found = false;
    int length = byteData.length() / sizeof( data[0] );
    for( int i=0; i<length; i++ )
        {
        EffectWindow* foundWin = effects->findWindow( data[i] );
        if( !foundWin )
            {
            kDebug(1212) << "Invalid window targetted for highlight. Requested:" << data[i];
            continue;
            }
        if( foundWin->isOnCurrentDesktop() && !foundWin->isMinimized() )
            m_highlightedWindows.append( foundWin );
        found = true;
        }
    if( !found )
        return;
    prepareHighlighting();
    m_windowOpacity[w] = 1.0; // Because it's not in stackingOrder() yet

    /* TODO: Finish thumbnails of offscreen windows, not sure if it's worth it though
    if( !m_highlightedWindow->isOnCurrentDesktop() )
        { // Window is offscreen, determine thumbnail position
        QRect screenArea = effects->clientArea( MaximizeArea ); // Workable area of the active screen
        QRect outerArea = outerArea.adjusted( outerArea.width() / 10, outerArea.height() / 10,
            -outerArea.width() / 10, -outerArea.height() / 10 ); // Add 10% margin around the edge
        QRect innerArea = outerArea.adjusted( outerArea.width() / 40, outerArea.height() / 40,
            -outerArea.width() / 40, -outerArea.height() / 40 ); // Outer edge of the thumbnail border (2.5%)
        QRect thumbArea = outerArea.adjusted( 20, 20, -20, -20 ); // Outer edge of the thumbnail (20px)

        // Determine the maximum size that we can make the thumbnail within the innerArea
        double areaAspect = double( thumbArea.width() ) / double( thumbArea.height() );
        double windowAspect = aspectRatio( m_highlightedWindow );
        QRect thumbRect; // Position doesn't matter right now, but it will later
        if( windowAspect > areaAspect )
            // Top/bottom will touch first
            thumbRect = QRect( 0, 0, widthForHeight( thumbArea.height() ), thumbArea.height() );
        else // Left/right will touch first
            thumbRect = QRect( 0, 0, thumbArea.width(), heightForWidth( thumbArea.width() ));
        if( thumbRect.width() >= m_highlightedWindow->width() )
            // Area is larger than the window, just use the window's size
            thumbRect = m_highlightedWindow->geometry();

        // Determine position of desktop relative to the current one
        QPoint direction = effects->desktopGridCoords( m_highlightedWindow->desktop() ) -
            effects->desktopGridCoords( effects->currentDesktop() );

        // Draw a line from the center of the current desktop to the center of the target desktop.
        QPointF desktopLine( 0, 0, direction.x() * screenArea.width(), direction.y() * screenArea.height() );
        desktopLeft.translate( screenArea.width() / 2, screenArea.height() / 2 ); // Move to the screen center

        // Take the point where the line crosses the outerArea, this will be the tip of our arrow
        QPointF arrowTip;
        QLineF testLine( // Top
            outerArea.x(), outerArea.y(),
            outerArea.x() + outerArea.width(), outerArea.y() );
        if( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
            {
            testLine = QLineF( // Right
                outerArea.x() + outerArea.width(), outerArea.y(),
                outerArea.x() + outerArea.width(), outerArea.y() + outerArea.height() );
            if( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
                {
                testLine = QLineF( // Bottom
                    outerArea.x() + outerArea.width(), outerArea.y() + outerArea.height(),
                    outerArea.x(), outerArea.y() + outerArea.height() );
                if( desktopLine.intersect( testLine, &arrowTip ) != QLineF::BoundedIntersection )
                    {
                    testLine = QLineF( // Left
                        outerArea.x(), outerArea.y() + outerArea.height(),
                        outerArea.x(), outerArea.y() );
                    desktopLine.intersect( testLine, &arrowTip ); // Should never fail
                    }
                }
            }
        m_arrowTip = arrowTip.toPoint();
        } */
    }

void HighlightWindowEffect::prepareHighlighting()
    {
    // Create window data for every window. Just calling [w] creates it.
    m_finishing = false;
    foreach( EffectWindow *w, effects->stackingOrder() )
        if( !m_windowOpacity.contains( w )) // Just in case we are still finishing from last time
            m_windowOpacity[w] = 1.0;
    }

void HighlightWindowEffect::finishHighlighting()
    {
    m_finishing = true;
    m_monitorWindow = NULL;
    m_highlightedWindows.clear();
    }

} // namespace
