/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_CUBE_H
#define KWIN_CUBE_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <kshortcut.h>
#include <QObject>
#include <QQueue>
#include <QSet>

namespace KWin
{

class CubeEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        CubeEffect();
        ~CubeEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual bool borderActivated( ElectricBorder border );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        virtual void mouseChanged( const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, 
            Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxUpdated();
        virtual void tabBoxClosed();
        virtual void windowAdded( EffectWindow* );

        static bool supported();
    private slots:
        void toggleCube();
        void toggleCylinder();
        void toggleSphere();
        // slots for global shortcut changed
        // needed to toggle the effect
        void cubeShortcutChanged( const QKeySequence& seq );
        void cylinderShortcutChanged( const QKeySequence& seq );
        void sphereShortcutChanged( const QKeySequence& seq );
    private:
        enum RotationDirection
            {
            Left,
            Right,
            Upwards,
            Downwards
            };
        enum VerticalRotationPosition
            {
            Up,
            Normal,
            Down
            };
        enum CubeMode
            {
            Cube,
            Cylinder,
            Sphere
            };
        void toggle( CubeMode newMode = Cube );
        void paintCube( int mask, QRegion region, ScreenPaintData& data );
        void paintCap();
        void paintCubeCap();
        void paintCylinderCap();
        void paintSphereCap();
        bool loadShader();
        void loadConfig( QString config );
        void rotateCube();
        void rotateToDesktop( int desktop );
        void setActive( bool active );
        bool activated;
        bool mousePolling;
        bool cube_painting;
        bool keyboard_grab;
        bool schedule_close;
        QList<ElectricBorder> borderActivate;
        QList<ElectricBorder> borderActivateCylinder;
        QList<ElectricBorder> borderActivateSphere;
        int painting_desktop;
        Window input;
        int frontDesktop;
        float cubeOpacity;
        bool opacityDesktopOnly;
        bool displayDesktopName;
        EffectFrame desktopNameFrame;
        QFont desktopNameFont;
        bool reflection;
        bool rotating;
        bool verticalRotating;
        bool desktopChangedWhileRotating;
        bool paintCaps;
        TimeLine timeLine;
        TimeLine verticalTimeLine;
        RotationDirection rotationDirection;
        RotationDirection verticalRotationDirection;
        VerticalRotationPosition verticalPosition;
        QQueue<RotationDirection> rotations;
        QQueue<RotationDirection> verticalRotations;
        QColor backgroundColor;
        QColor capColor;
        GLTexture* wallpaper;
        bool texturedCaps;
        GLTexture* capTexture;
        float manualAngle;
        float manualVerticalAngle;
        TimeLine::CurveShape currentShape;
        bool start;
        bool stop;
        bool reflectionPainting;
        int rotationDuration;
        int activeScreen;
        bool bottomCap;
        bool closeOnMouseRelease;
        float zoom;
        float zPosition;
        bool useForTabBox;
        bool invertKeys;
        bool invertMouse;
        bool tabBoxMode;
        bool shortcutsRegistered;
        CubeMode mode;
        bool useShaders;
        GLShader* cylinderShader;
        GLShader* sphereShader;
        float capDeformationFactor;
        bool useZOrdering;
        float zOrderingFactor;
        bool useList;
        // needed for reflection
        float mAddedHeightCoeff1;
        float mAddedHeightCoeff2;

        // GL lists
        bool capListCreated;
        bool recompileList;
        GLuint glList;

        // Shortcuts - needed to toggle the effect
        KShortcut cubeShortcut;
        KShortcut cylinderShortcut;
        KShortcut sphereShortcut;
    };

} // namespace

#endif
