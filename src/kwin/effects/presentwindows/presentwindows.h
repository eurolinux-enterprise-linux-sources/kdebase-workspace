/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

#include "presentwindows_proxy.h"

#include <kwineffects.h>
#include <kshortcut.h>

namespace KWin
{

/**
 * Expose-like effect which shows all windows on current desktop side-by-side,
 *  letting the user select active window.
 **/
class PresentWindowsEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    private:
        // Structures
        struct WindowData
            {
            bool visible;
            bool deleted;
            bool referenced;
            double opacity;
            double highlight;
            int slot;
            int slot_distance;
            EffectFrame* textFrame;
            EffectFrame* iconFrame;
            };
        typedef QHash<EffectWindow*, WindowData> DataHash;
        struct GridSize
            {
            int columns;
            int rows;
            };

    public:
        PresentWindowsEffect();
        virtual ~PresentWindowsEffect();

        virtual void reconfigure( ReconfigureFlags );
        virtual const void* proxy() const;

        // Screen painting
        virtual void prePaintScreen( ScreenPrePaintData &data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData &data );
        virtual void postPaintScreen();

        // Window painting
        virtual void prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time );
        virtual void paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data );

        // User interaction
        virtual void windowAdded( EffectWindow *w );
        virtual void windowClosed( EffectWindow *w );
        virtual void windowDeleted( EffectWindow *w );
        virtual bool borderActivated( ElectricBorder border );
        virtual void windowInputMouseEvent( Window w, QEvent *e );
        virtual void grabbedKeyboardEvent( QKeyEvent *e );

        // Tab box
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();

        enum { LayoutNatural, LayoutRegularGrid, LayoutFlexibleGrid }; // Layout modes

    public slots:
        void setActive( bool active, bool closingTab = false ); // HACK: closingTab shouldn't be needed
        void toggleActive()  { m_allDesktops = false; setActive( !m_activated ); }
        void toggleActiveAllDesktops()  { m_allDesktops = true; setActive( !m_activated ); }

        // slots for global shortcut changed
        // needed to toggle the effect
        void globalShortcutChanged( const QKeySequence& seq );
        void globalShortcutChangedAll( const QKeySequence& seq );

    protected:
        // Window rearranging
        void rearrangeWindows();
        void calculateWindowTransformationsClosest( EffectWindowList windowlist, int screen );
        void calculateWindowTransformationsKompose( EffectWindowList windowlist, int screen );
        void calculateWindowTransformationsNatural( EffectWindowList windowlist, int screen );

        // Helper functions for window rearranging
        inline double aspectRatio( EffectWindow *w )
            { return w->width() / double( w->height() ); }
        inline int widthForHeight( EffectWindow *w, int height )
            { return int(( height / double( w->height() )) * w->width() ); }
        inline int heightForWidth( EffectWindow *w, int width )
            { return int(( width / double( w->width() )) * w->height() ); }
        void assignSlots( EffectWindowList windowlist, const QRect &area, int columns, int rows );
        void getBestAssignments( EffectWindowList windowlist );
        bool isOverlappingAny( EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border );

        // Filter box
        void updateFilterFrame();

        // Helper functions
        bool isSelectableWindow( EffectWindow *w );
        bool isVisibleWindow( EffectWindow *w );
        void setHighlightedWindow( EffectWindow *w );
        EffectWindow* relativeWindow( EffectWindow *w, int xdiff, int ydiff, bool wrap ) const;
        EffectWindow* findFirstWindow() const;

    private:
        PresentWindowsEffectProxy m_proxy;
        friend class PresentWindowsEffectProxy;

        // User configuration settings
        QList<ElectricBorder> m_borderActivate;
        QList<ElectricBorder> m_borderActivateAll;
        int m_layoutMode;
        bool m_showCaptions;
        bool m_showIcons;
        bool m_tabBoxAllowed;
        int m_accuracy;
        bool m_fillGaps;
        double m_fadeDuration;
        bool m_showPanel;

        // Activation
        bool m_activated;
        bool m_allDesktops;
        bool m_ignoreMinimized;
        double m_decalOpacity;
        Window m_input;
        bool m_hasKeyboardGrab;
        bool m_tabBoxEnabled;

        // Window data
        WindowMotionManager m_motionManager;
        DataHash m_windowData;
        EffectWindow *m_highlightedWindow;

        // Grid layout info
        QList<GridSize> m_gridSizes;

        // Filter box
        EffectFrame m_filterFrame;
        QString m_windowFilter;

        // Shortcut - needed to toggle the effect
        KShortcut shortcut;
        KShortcut shortcutAll;
    };

} // namespace

#endif
