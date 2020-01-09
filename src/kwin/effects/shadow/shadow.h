/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>
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

#ifndef KWIN_SHADOW_H
#define KWIN_SHADOW_H

#include <kwineffects.h>
#include <kwinxrenderutils.h>

namespace KWin
{

class GLTexture;

class ShadowEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShadowEffect();
        virtual ~ShadowEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void windowClosed( EffectWindow* c );
        virtual void buildQuads( EffectWindow* w, WindowQuadList& quadList );
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );

    private slots:
        void updateShadowColor();

    private:
        void prepareRenderStates( GLTexture *texture, double opacity, double brightness, double saturation );
        void restoreRenderStates( GLTexture *texture, double opacity, double brightness, double saturation );

        void drawShadowQuadOpenGL( GLTexture *texture, QVector<float> verts, QVector<float> texCoords,
            QRegion region, float opacity, float brightness, float saturation, GLShader* shader );
        void drawShadowQuadXRender( XRenderPicture *picture, QRect rect, float xScale, float yScale,
            QColor color, float opacity, float brightness, float saturation );

        void drawShadow( EffectWindow* w, int mask, QRegion region, const WindowPaintData& data );
        void addQuadVertices(QVector<float>& verts, float x1, float y1, float x2, float y2) const;
        // transforms window rect -> shadow rect
        QRect shadowRectangle( EffectWindow* w, const QRect& windowRectangle ) const;
        bool useShadow( EffectWindow* w ) const;
        void drawQueuedShadows( EffectWindow* behindWindow );

        int shadowXOffset, shadowYOffset;
        double shadowOpacity;
        int shadowFuzzyness;
        int shadowSize;
        bool intensifyActiveShadow;
        QColor shadowColor, cachedColor;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        QList<GLTexture*> mDefaultShadowTextures;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        QList<XRenderPicture*> mDefaultShadowPics;
        XRenderPicture cachedBlendPicture;
#endif

        WindowQuadType mDefaultShadowQuadType;

        struct ShadowData
        {
            ShadowData(EffectWindow* _w, WindowPaintData& _data) : w(_w), data(_data) {}
            EffectWindow* w;
            QRegion clip;
            int mask;
            QRegion region;
            WindowPaintData data;
        };

        QList<ShadowData> shadowDatas;
    };

} // namespace

#endif
