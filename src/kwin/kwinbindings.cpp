/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

// This file is #included from within:
// Workspace::initShortcuts()
//  {

// Some shortcuts have Tarzan-speech like names, they need extra
// normal human descriptions with DEF2() the others can use DEF()
#ifndef NOSLOTS
# define DEF2( name, descr, key, fnSlot )                           \
   a = actionCollection->addAction( name );                         \
   a->setText( i18n(descr) );                                       \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));  \
   connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot))
# define DEF( name, key, fnSlot )                                   \
   a = actionCollection->addAction( name );                         \
   a->setText( i18n(name) );                                        \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));  \
   connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot))
#else
# define DEF2( name, descr, key, fnSlot )                           \
   a = actionCollection->addAction( name );                         \
   a->setText( i18n(descr) );                                       \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));
# define DEF( name, key, fnSlot )                                   \
   a = actionCollection->addAction( name );                         \
   a->setText( i18n(name) );                                        \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));
#endif

    a = actionCollection->addAction( "Program:kwin" );
    a->setText( i18n("System") );

    a = actionCollection->addAction( "Group:Navigation" );
    a->setText( i18n("Navigation") );
    DEF( I18N_NOOP("Walk Through Windows"),                 Qt::ALT+Qt::Key_Tab, slotWalkThroughWindows() );
    DEF( I18N_NOOP("Walk Through Windows (Reverse)"),       Qt::ALT+Qt::SHIFT+Qt::Key_Backtab, slotWalkBackThroughWindows() );
    DEF( I18N_NOOP("Walk Through Desktops"),                0, slotWalkThroughDesktops() );
    DEF( I18N_NOOP("Walk Through Desktops (Reverse)"),      0, slotWalkBackThroughDesktops() );
    DEF( I18N_NOOP("Walk Through Desktop List"),            0, slotWalkThroughDesktopList() );
    DEF( I18N_NOOP("Walk Through Desktop List (Reverse)"),  0, slotWalkBackThroughDesktopList() );

    a = actionCollection->addAction( "Group:Windows" );
    a->setText( i18n("Windows") );
    DEF( I18N_NOOP("Window Operations Menu"),
        Qt::ALT+Qt::Key_F3, slotWindowOperations() );
    DEF2( "Window Close", I18N_NOOP("Close Window"),
        Qt::ALT+Qt::Key_F4, slotWindowClose() );
    DEF2( "Window Maximize", I18N_NOOP("Maximize Window"),
        0, slotWindowMaximize() );
    DEF2( "Window Maximize Vertical", I18N_NOOP("Maximize Window Vertically"),
        0, slotWindowMaximizeVertical() );
    DEF2( "Window Maximize Horizontal", I18N_NOOP("Maximize Window Horizontally"),
        0, slotWindowMaximizeHorizontal() );
    DEF2( "Window Minimize", I18N_NOOP("Minimize Window"),
        0, slotWindowMinimize() );
    DEF2( "Window Shade", I18N_NOOP("Shade Window"),
        0, slotWindowShade() );
    DEF2( "Window Move", I18N_NOOP("Move Window"),
        0, slotWindowMove() );
    DEF2( "Window Resize", I18N_NOOP("Resize Window"),
        0, slotWindowResize() );
    DEF2( "Window Raise", I18N_NOOP("Raise Window"),
        0, slotWindowRaise() );
    DEF2( "Window Lower", I18N_NOOP("Lower Window"),
        0, slotWindowLower() );
    DEF( I18N_NOOP("Toggle Window Raise/Lower"),
        0, slotWindowRaiseOrLower() );
    DEF2( "Window Fullscreen", I18N_NOOP("Make Window Fullscreen"),
        0, slotWindowFullScreen() );
    DEF2( "Window No Border", I18N_NOOP("Hide Window Border"),
        0, slotWindowNoBorder() );
    DEF2( "Window Above Other Windows", I18N_NOOP("Keep Window Above Others"),
        0, slotWindowAbove() );
    DEF2( "Window Below Other Windows", I18N_NOOP("Keep Window Below Others"),
        0, slotWindowBelow() );
    DEF( I18N_NOOP("Activate Window Demanding Attention"),
        Qt::CTRL+Qt::ALT+Qt::Key_A, slotActivateAttentionWindow());
    DEF( I18N_NOOP("Setup Window Shortcut"),
        0, slotSetupWindowShortcut());
    DEF2( "Window Pack Right", I18N_NOOP("Pack Window to the Right"),
        0, slotWindowPackRight() );
    DEF2( "Window Pack Left", I18N_NOOP("Pack Window to the Left"),
        0, slotWindowPackLeft() );
    DEF2( "Window Pack Up", I18N_NOOP("Pack Window Up"),
        0, slotWindowPackUp() );
    DEF2( "Window Pack Down", I18N_NOOP("Pack Window Down"),
        0, slotWindowPackDown() );
    DEF2( "Window Grow Horizontal", I18N_NOOP("Pack Grow Window Horizontally"),
        0, slotWindowGrowHorizontal() );
    DEF2( "Window Grow Vertical", I18N_NOOP("Pack Grow Window Vertically"),
        0, slotWindowGrowVertical() );
    DEF2( "Window Shrink Horizontal", I18N_NOOP("Pack Shrink Window Horizontally"),
        0, slotWindowShrinkHorizontal() );
    DEF2( "Window Shrink Vertical", I18N_NOOP("Pack Shrink Window Vertically"),
        0, slotWindowShrinkVertical() );

    a = actionCollection->addAction( "Group:Window Desktop" );
    a->setText( i18n("Window & Desktop") );
    DEF2( "Window On All Desktops", I18N_NOOP("Keep Window on All Desktops"),
        0, slotWindowOnAllDesktops() );
    DEF( I18N_NOOP("Window to Desktop 1"),              0, slotWindowToDesktop1() );
    DEF( I18N_NOOP("Window to Desktop 2"),              0, slotWindowToDesktop2() );
    DEF( I18N_NOOP("Window to Desktop 3"),              0, slotWindowToDesktop3() );
    DEF( I18N_NOOP("Window to Desktop 4"),              0, slotWindowToDesktop4() );
    DEF( I18N_NOOP("Window to Desktop 5"),              0, slotWindowToDesktop5() );
    DEF( I18N_NOOP("Window to Desktop 6"),              0, slotWindowToDesktop6() );
    DEF( I18N_NOOP("Window to Desktop 7"),              0, slotWindowToDesktop7() );
    DEF( I18N_NOOP("Window to Desktop 8"),              0, slotWindowToDesktop8() );
    DEF( I18N_NOOP("Window to Desktop 9"),              0, slotWindowToDesktop9() );
    DEF( I18N_NOOP("Window to Desktop 10"),             0, slotWindowToDesktop10() );
    DEF( I18N_NOOP("Window to Desktop 11"),             0, slotWindowToDesktop11() );
    DEF( I18N_NOOP("Window to Desktop 12"),             0, slotWindowToDesktop12() );
    DEF( I18N_NOOP("Window to Desktop 13"),             0, slotWindowToDesktop13() );
    DEF( I18N_NOOP("Window to Desktop 14"),             0, slotWindowToDesktop14() );
    DEF( I18N_NOOP("Window to Desktop 15"),             0, slotWindowToDesktop15() );
    DEF( I18N_NOOP("Window to Desktop 16"),             0, slotWindowToDesktop16() );
    DEF( I18N_NOOP("Window to Desktop 17"),             0, slotWindowToDesktop17() );
    DEF( I18N_NOOP("Window to Desktop 18"),             0, slotWindowToDesktop18() );
    DEF( I18N_NOOP("Window to Desktop 19"),             0, slotWindowToDesktop19() );
    DEF( I18N_NOOP("Window to Desktop 20"),             0, slotWindowToDesktop20() );
    DEF( I18N_NOOP("Window to Next Desktop"),           0, slotWindowToNextDesktop() );
    DEF( I18N_NOOP("Window to Previous Desktop"),       0, slotWindowToPreviousDesktop() );
    DEF( I18N_NOOP("Window One Desktop to the Right"),  0, slotWindowToDesktopRight() );
    DEF( I18N_NOOP("Window One Desktop to the Left"),   0, slotWindowToDesktopLeft() );
    DEF( I18N_NOOP("Window One Desktop Up"),            0, slotWindowToDesktopUp() );
    DEF( I18N_NOOP("Window One Desktop Down"),          0, slotWindowToDesktopDown() );
    DEF( I18N_NOOP("Window to Screen 0"),               0, slotWindowToScreen0() );
    DEF( I18N_NOOP("Window to Screen 1"),               0, slotWindowToScreen1() );
    DEF( I18N_NOOP("Window to Screen 2"),               0, slotWindowToScreen2() );
    DEF( I18N_NOOP("Window to Screen 3"),               0, slotWindowToScreen3() );
    DEF( I18N_NOOP("Window to Screen 4"),               0, slotWindowToScreen4() );
    DEF( I18N_NOOP("Window to Screen 5"),               0, slotWindowToScreen5() );
    DEF( I18N_NOOP("Window to Screen 6"),               0, slotWindowToScreen6() );
    DEF( I18N_NOOP("Window to Screen 7"),               0, slotWindowToScreen7() );
    DEF( I18N_NOOP("Window to Next Screen"),            0, slotWindowToNextScreen() );

    a = actionCollection->addAction( "Group:Desktop Switching" );
    a->setText( i18n("Desktop Switching") );
    DEF( I18N_NOOP("Switch to Desktop 1"),              Qt::CTRL+Qt::Key_F1, slotSwitchToDesktop1() );
    DEF( I18N_NOOP("Switch to Desktop 2"),              Qt::CTRL+Qt::Key_F2, slotSwitchToDesktop2() );
    DEF( I18N_NOOP("Switch to Desktop 3"),              Qt::CTRL+Qt::Key_F3, slotSwitchToDesktop3() );
    DEF( I18N_NOOP("Switch to Desktop 4"),              Qt::CTRL+Qt::Key_F4, slotSwitchToDesktop4() );
    DEF( I18N_NOOP("Switch to Desktop 5"),              0, slotSwitchToDesktop5() );
    DEF( I18N_NOOP("Switch to Desktop 6"),              0, slotSwitchToDesktop6() );
    DEF( I18N_NOOP("Switch to Desktop 7"),              0, slotSwitchToDesktop7() );
    DEF( I18N_NOOP("Switch to Desktop 8"),              0, slotSwitchToDesktop8() );
    DEF( I18N_NOOP("Switch to Desktop 9"),              0, slotSwitchToDesktop9() );
    DEF( I18N_NOOP("Switch to Desktop 10"),             0, slotSwitchToDesktop10() );
    DEF( I18N_NOOP("Switch to Desktop 11"),             0, slotSwitchToDesktop11() );
    DEF( I18N_NOOP("Switch to Desktop 12"),             0, slotSwitchToDesktop12() );
    DEF( I18N_NOOP("Switch to Desktop 13"),             0, slotSwitchToDesktop13() );
    DEF( I18N_NOOP("Switch to Desktop 14"),             0, slotSwitchToDesktop14() );
    DEF( I18N_NOOP("Switch to Desktop 15"),             0, slotSwitchToDesktop15() );
    DEF( I18N_NOOP("Switch to Desktop 16"),             0, slotSwitchToDesktop16() );
    DEF( I18N_NOOP("Switch to Desktop 17"),             0, slotSwitchToDesktop17() );
    DEF( I18N_NOOP("Switch to Desktop 18"),             0, slotSwitchToDesktop18() );
    DEF( I18N_NOOP("Switch to Desktop 19"),             0, slotSwitchToDesktop19() );
    DEF( I18N_NOOP("Switch to Desktop 20"),             0, slotSwitchToDesktop20() );
    DEF( I18N_NOOP("Switch to Next Desktop"),           0, slotSwitchDesktopNext() );
    DEF( I18N_NOOP("Switch to Previous Desktop"),       0, slotSwitchDesktopPrevious() );
    DEF( I18N_NOOP("Switch One Desktop to the Right"),  0, slotSwitchDesktopRight() );
    DEF( I18N_NOOP("Switch One Desktop to the Left"),   0, slotSwitchDesktopLeft() );
    DEF( I18N_NOOP("Switch One Desktop Up"),            0, slotSwitchDesktopUp() );
    DEF( I18N_NOOP("Switch One Desktop Down"),          0, slotSwitchDesktopDown() );
    DEF( I18N_NOOP("Switch to Screen 0"),               0, slotSwitchToScreen0() );
    DEF( I18N_NOOP("Switch to Screen 1"),               0, slotSwitchToScreen1() );
    DEF( I18N_NOOP("Switch to Screen 2"),               0, slotSwitchToScreen2() );
    DEF( I18N_NOOP("Switch to Screen 3"),               0, slotSwitchToScreen3() );
    DEF( I18N_NOOP("Switch to Screen 4"),               0, slotSwitchToScreen4() );
    DEF( I18N_NOOP("Switch to Screen 5"),               0, slotSwitchToScreen5() );
    DEF( I18N_NOOP("Switch to Screen 6"),               0, slotSwitchToScreen6() );
    DEF( I18N_NOOP("Switch to Screen 7"),               0, slotSwitchToScreen7() );
    DEF( I18N_NOOP("Switch to Next Screen"),            0, slotSwitchToNextScreen() );

    a = actionCollection->addAction( "Group:Miscellaneous" );
    a->setText( i18n("Miscellaneous") );
    DEF( I18N_NOOP("Mouse Emulation"),                  Qt::ALT+Qt::Key_F12, slotMouseEmulation() );
    DEF( I18N_NOOP("Kill Window"),                      Qt::CTRL+Qt::ALT+Qt::Key_Escape, slotKillWindow() );
    DEF( I18N_NOOP("Window Screenshot to Clipboard"),   Qt::ALT+Qt::Key_Print, slotGrabWindow() );
    DEF( I18N_NOOP("Desktop Screenshot to Clipboard"),  Qt::CTRL+Qt::Key_Print, slotGrabDesktop() );
    DEF( I18N_NOOP("Block Global Shortcuts"),           0, slotDisableGlobalShortcuts());
    DEF( I18N_NOOP("Suspend Compositing"),              Qt::SHIFT+Qt::ALT+Qt::Key_F12, slotToggleCompositing());

#undef DEF
#undef DEF2

//  }
