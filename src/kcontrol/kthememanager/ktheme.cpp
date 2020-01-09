// -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4; -*-
/*  Copyright (C) 2003 Lukas Tinkl <lukas@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ktheme.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QRegExp>
#include <QTextStream>
#include <QDir>

#include <kapplication.h>
#include <kbuildsycocaprogressdialog.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <klocale.h>
#include <kicontheme.h>
#include <kservice.h>
#include <kstandarddirs.h>
#include <ktar.h>
#include <kstyle.h>
#include <QX11Info>
#include <QtDBus/QtDBus>
#include <kglobalsettings.h>
#include "kwin_interface.h"
#include "plasma_interface.h"

KTheme::KTheme( QWidget *parent, const QString & xmlFile )
    : m_parent(parent)
{
    QFile file( xmlFile );
    file.open( QIODevice::ReadOnly );
    m_dom.setContent( file.readAll() );
    file.close();

    //kDebug() << m_dom.toString( 2 );

    setName( QFileInfo( file ).baseName() );
    m_kgd = KGlobal::dirs();
}

KTheme::KTheme( QWidget *parent, bool create )
    : m_parent(parent)
{
    if ( create )
    {
        m_dom = QDomDocument( "ktheme" );

        m_root = m_dom.createElement( "ktheme" );
        m_root.setAttribute( "version", SYNTAX_VERSION );
        m_dom.appendChild( m_root );

        m_general = m_dom.createElement( "general" );
        m_root.appendChild( m_general );
    }

    m_kgd = KGlobal::dirs();
}

KTheme::~KTheme()
{
}

void KTheme::setName( const QString & name )
{
    m_name = name;
}

bool KTheme::load( const KUrl & url )
{
    kDebug() << "Loading theme from URL: " << url;

    QString tmpFile;
    if ( !KIO::NetAccess::download( url, tmpFile, 0L ) )
        return false;

    kDebug() << "Theme is in temp file: " << tmpFile;

    // set theme's name
    setName( QFileInfo( url.fileName() ).baseName() );

    // unpack the tarball
    QString location = m_kgd->saveLocation( "themes", m_name + '/' );
    KTar tar( tmpFile );
    tar.open( QIODevice::ReadOnly );
    tar.directory()->copyTo( location );
    tar.close();

    // create the DOM
    QFile file( location + m_name + ".xml" );
    file.open( QIODevice::ReadOnly );
    m_dom.setContent( file.readAll() );
    file.close();

    // remove the temp file
    KIO::NetAccess::removeTempFile( tmpFile );

    return true;
}

QString KTheme::createYourself( bool pack )
{
    // start with empty dir for orig theme
    if ( !pack )
        KTheme::remove( name() );

    // 1. General stuff set by methods setBlah()

    // 2. Background theme
    KSharedConfig::Ptr globalConf = KGlobal::config();

    KConfig _kwinConf( "kwinrc" );
    KConfigGroup kwinConf(&_kwinConf, "Desktops" );
    uint numDesktops = kwinConf.readEntry( "Number", 4 );

    KConfig _desktopConf( "kdesktoprc" );
    KConfigGroup desktopConf(&_desktopConf, "Background Common" );
    bool common = desktopConf.readEntry( "CommonDesktop", true );

    for ( uint i=0; i < numDesktops-1; i++ )
    {
        QDomElement desktopElem = m_dom.createElement( "desktop" );
        desktopElem.setAttribute( "number", i );
        desktopElem.setAttribute( "common", common );
        
        KConfigGroup group(&_desktopConf,"Desktop" + QString::number( i ));

        QDomElement modeElem = m_dom.createElement( "mode" );
        modeElem.setAttribute( "id", group.readEntry( "BackgroundMode", "Flat" ) );
        desktopElem.appendChild( modeElem );

        QDomElement c1Elem = m_dom.createElement( "color1" );
        c1Elem.setAttribute( "rgb", group.readEntry( "Color1",QColor() ).name() );
        desktopElem.appendChild( c1Elem );

        QDomElement c2Elem = m_dom.createElement( "color2" );
        c2Elem.setAttribute( "rgb", group.readEntry( "Color2",QColor() ).name() );
        desktopElem.appendChild( c2Elem );

        QDomElement blendElem = m_dom.createElement( "blending" );
        blendElem.setAttribute( "mode", group.readEntry( "BlendMode", QString( "NoBlending" ) ) );
        blendElem.setAttribute( "balance", group.readEntry( "BlendBalance", QString::number( 100 ) ) );
        blendElem.setAttribute( "reverse", group.readEntry( "ReverseBlending", false ) );
        desktopElem.appendChild( blendElem );

        QDomElement patElem = m_dom.createElement( "pattern" );
        patElem.setAttribute( "name", group.readEntry( "Pattern" ) );
        desktopElem.appendChild( patElem );

        QDomElement wallElem = m_dom.createElement( "wallpaper" );
        wallElem.setAttribute( "url", processFilePath( "desktop", group.readPathEntry( "Wallpaper", QString() ) ) );
        wallElem.setAttribute( "mode", group.readEntry( "WallpaperMode" ) );
        desktopElem.appendChild( wallElem );

        // TODO handle multi wallpapers (aka slideshow)

        m_root.appendChild( desktopElem );

        if ( common )           // generate only one node
            break;
    }

    // 11. Screensaver
    KConfigGroup saver(&_desktopConf,"ScreenSaver" );
    QDomElement saverElem = m_dom.createElement( "screensaver" );
    saverElem.setAttribute( "name", saver.readEntry( "Saver" ) );
    m_root.appendChild( saverElem );

    // 3. Icons
    QDomElement iconElem = m_dom.createElement( "icons" );
    iconElem.setAttribute("name", globalConf->group("Icons").readEntry("Theme",KIconTheme::current()));
    createIconElems( globalConf->group("DesktopIcons"), "desktop", iconElem );
    createIconElems( globalConf->group("MainToolbarIcons"), "mainToolbar", iconElem );
    createIconElems( globalConf->group("PanelIcons"), "panel", iconElem );
    createIconElems( globalConf->group("SmallIcons"), "small", iconElem );
    createIconElems( globalConf->group("ToolbarIcons"), "toolbar", iconElem );
    m_root.appendChild( iconElem );

    // 4. Sounds
    // 4.1 Global sounds
    KConfig * soundConf = new KConfig( "knotify.eventsrc" );
    QStringList stdEvents;
    stdEvents << "cannotopenfile" << "catastrophe" << "exitkde" << "fatalerror"
              << "notification" << "printerror" << "startkde" << "warning"
              << "messageCritical" << "messageInformation" << "messageWarning"
              << "messageboxQuestion";

    // 4.2 KWin sounds
    KConfig * kwinSoundConf = new KConfig( "kwin.eventsrc" );
    QStringList kwinEvents;
    kwinEvents << "activate" << "close" << "delete" <<
        "desktop1" << "desktop2" << "desktop3" << "desktop4" <<
        "desktop5" << "desktop6" << "desktop7" << "desktop8" <<
        "maximize" << "minimize" << "moveend" << "movestart" <<
        "new" << "not_on_all_desktops" << "on_all_desktops" <<
        "resizeend" << "resizestart" << "shadedown" << "shadeup" <<
        "transdelete" << "transnew" << "unmaximize" << "unminimize";

    QDomElement soundsElem = m_dom.createElement( "sounds" );
    createSoundList( stdEvents, "global", soundsElem, soundConf );
    createSoundList( kwinEvents, "kwin", soundsElem, kwinSoundConf );
    m_root.appendChild( soundsElem );
    delete soundConf;
    delete kwinSoundConf;


    // 5. Colors
    QDomElement colorsElem = m_dom.createElement( "colors" );
    colorsElem.setAttribute( "contrast", globalConf->group("KDE").readEntry( "contrast", 7 ) );
    QStringList stdColors;
    stdColors << "background" << "selectBackground" << "foreground" << "windowForeground"
              << "windowBackground" << "selectForeground" << "buttonBackground"
              << "buttonForeground" << "linkColor" << "visitedLinkColor" << "alternateBackground";

    KConfigGroup generalGroup(globalConf, "General");
    for ( QStringList::const_iterator it = stdColors.begin(); it != stdColors.end(); ++it )
        createColorElem( ( *it ), "global", colorsElem, generalGroup );

    QStringList kwinColors;
    kwinColors << "activeForeground" << "inactiveBackground" << "inactiveBlend" << "activeBackground"
               << "activeBlend" << "inactiveForeground" << "activeTitleBtnBg" << "inactiveTitleBtnBg"
               << "frame" << "inactiveFrame" << "handle" << "inactiveHandle";
    KConfigGroup wmGroup(globalConf, "WM");
    for ( QStringList::const_iterator it = kwinColors.begin(); it != kwinColors.end(); ++it )
        createColorElem( ( *it ), "kwin", colorsElem, wmGroup );

    m_root.appendChild( colorsElem );

    // 6. Cursors
    KConfig* mouseConf = new KConfig( "kcminputrc" );
    QDomElement cursorsElem = m_dom.createElement( "cursors" );
    cursorsElem.setAttribute( "name", mouseConf->group("Mouse").readEntry( "cursorTheme" ) );
    m_root.appendChild( cursorsElem );
    delete mouseConf;
    // TODO copy the cursor theme?

    // 7. KWin
    kwinConf.changeGroup( "Style" );
    QDomElement wmElem = m_dom.createElement( "wm" );
    wmElem.setAttribute( "name", kwinConf.readEntry( "PluginLib" ) );
    wmElem.setAttribute( "type", "builtin" );    // TODO support pixmap themes when the kwin client gets ported
    if ( kwinConf.readEntry( "CustomButtonPositions" , false)  )
    {
        QDomElement buttonsElem = m_dom.createElement( "buttons" );
        buttonsElem.setAttribute( "left", kwinConf.readEntry( "ButtonsOnLeft" ) );
        buttonsElem.setAttribute( "right", kwinConf.readEntry( "ButtonsOnRight" ) );
        wmElem.appendChild( buttonsElem );
    }
    QDomElement borderElem = m_dom.createElement( "border" );
    borderElem.setAttribute( "size", kwinConf.readEntry( "BorderSize", 1 ) );
    wmElem.appendChild( borderElem );
    m_root.appendChild( wmElem );

    // 8. Konqueror
    KConfig _konqConf( "konquerorrc" );
    KConfigGroup konqConf(&_konqConf, "Settings" );
    QDomElement konqElem = m_dom.createElement( "konqueror" );
    QDomElement konqWallElem = m_dom.createElement( "wallpaper" );
    QString bgImagePath = konqConf.readPathEntry( "BgImage", QString() );
    konqWallElem.setAttribute( "url", processFilePath( "konqueror", bgImagePath ) );
    konqElem.appendChild( konqWallElem );
    QDomElement konqBgColorElem = m_dom.createElement( "bgcolor" );
    konqBgColorElem.setAttribute( "rgb", konqConf.readEntry( "BgColor",QColor() ).name() );
    konqElem.appendChild( konqBgColorElem );
    m_root.appendChild( konqElem );
#if 0
    // 9. Kicker (aka KDE Panel)
    KConfig _kickerConf( "kickerrc" );
    KConfigGroup kickerConf(&_kickerConf, "General" );

    QDomElement panelElem = m_dom.createElement( "panel" );

    if ( kickerConf.readEntry( "UseBackgroundTheme" , false) )
    {
        QDomElement backElem = m_dom.createElement( "background" );
        QString kbgPath = kickerConf.readPathEntry( "BackgroundTheme", QString() );
        backElem.setAttribute( "url", processFilePath( "panel", kbgPath ) );
        backElem.setAttribute( "colorize", kickerConf.readEntry( "ColorizeBackground" , false) );
        panelElem.appendChild( backElem );
    }

    QDomElement transElem = m_dom.createElement( "transparent" );
    transElem.setAttribute( "value", kickerConf.readEntry( "Transparent" , false) );
    panelElem.appendChild( transElem );

    QDomElement posElem = m_dom.createElement( "position" );
    posElem.setAttribute( "value", kickerConf.readEntry( "Position" ) );
    panelElem.appendChild( posElem );


    QDomElement showLeftHideButtonElem = m_dom.createElement( "showlefthidebutton" );
    showLeftHideButtonElem.setAttribute( "value", kickerConf.readEntry( "ShowLeftHideButton",true ) );
    panelElem.appendChild( showLeftHideButtonElem );

    QDomElement showRightHideButtonElem = m_dom.createElement( "showrighthidebutton" );
    showRightHideButtonElem.setAttribute( "value", kickerConf.readEntry( "ShowRightHideButton",true ) );
    panelElem.appendChild( showRightHideButtonElem );



    m_root.appendChild( panelElem );
#endif
    // 10. Widget style
    QDomElement widgetsElem = m_dom.createElement( "widgets" );
    widgetsElem.setAttribute( "name", globalConf->group("General").readEntry( "widgetStyle", KStyle::defaultStyle() ) );
    m_root.appendChild( widgetsElem );

    // 12.
    QDomElement fontsElem = m_dom.createElement( "fonts" );
    QStringList fonts;
    fonts   << "General"    << "font"
            << "General"    << "fixed"
            << "General"    << "toolBarFont"
            << "General"    << "menuFont"
            << "WM"         << "activeFont"
            << "General"    << "taskbarFont"
            << "FMSettings" << "StandardFont";

    for ( QStringList::const_iterator it = fonts.begin(); it != fonts.end(); ++it ) {
        QString group = *it; ++it;
        QString key   = *it;
        QString value;

        if ( group == "FMSettings" ) {
            KConfigGroup grp(&_desktopConf,group );
            value = grp.readEntry( key, QString() );
        }
        else {
            value = globalConf->group(group).readEntry( key, QString() );
        }
        QDomElement fontElem = m_dom.createElement( key );
        fontElem.setAttribute( "object", group );
        fontElem.setAttribute( "value", value );
        fontsElem.appendChild( fontElem );
    }
    m_root.appendChild( fontsElem );

    // Save the XML
    QFile file( m_kgd->saveLocation( "themes", m_name + '/' ) + m_name + ".xml" );
    if ( file.open( QIODevice::WriteOnly ) ) {
        QTextStream stream( &file );
        m_dom.save( stream, 2 );
        file.close();
    }

    QString result;
    if ( pack )
    {
        // Pack the whole theme
        KTar tar( m_kgd->saveLocation( "themes" ) + m_name + ".kth", "application/x-gzip" );
        tar.open( QIODevice::WriteOnly );

        kDebug() << "Packing everything under: " << m_kgd->saveLocation( "themes", m_name + '/' );

        if ( tar.addLocalDirectory( m_kgd->saveLocation( "themes", m_name + '/' ), QString() ) )
            result = tar.fileName();

        tar.close();
    }

    //kDebug() << m_dom.toString( 2 );

    return result;
}

void KTheme::apply()
{
    kDebug() << "Going to apply theme: " << m_name;

    QString themeDir = m_kgd->findResourceDir( "themes", m_name + '/' + m_name + ".xml") + m_name + '/';
    kDebug() << "Theme dir: " << themeDir;

    // 2. Background theme

    QDomNodeList desktopList = m_dom.elementsByTagName( "desktop" );
    KConfig _desktopConf( "kdesktoprc" );
    KConfigGroup desktopConf(&_desktopConf, "Background Common" );

    for ( int i = 0; i <= desktopList.count(); i++ )
    {
        QDomElement desktopElem = desktopList.item( i ).toElement();
        if ( !desktopElem.isNull() )
        {
            // TODO optimize, don't write several times the common section
            bool common = static_cast<bool>( desktopElem.attribute( "common", "true" ).toUInt() );
            desktopConf.writeEntry( "CommonDesktop", common );
            desktopConf.writeEntry( "DeskNum", desktopElem.attribute( "number", "0" ).toUInt() );

            KConfigGroup grp(&_desktopConf, QString( "Desktop%1" ).arg( i ) );
            grp.writeEntry( "BackgroundMode", getProperty( desktopElem, "mode", "id" ) );
            grp.writeEntry( "Color1", QColor( getProperty( desktopElem, "color1", "rgb" ) ) );
            grp.writeEntry( "Color2", QColor( getProperty( desktopElem, "color2", "rgb" ) ) );
            grp.writeEntry( "BlendMode", getProperty( desktopElem, "blending", "mode" ) );
            grp.writeEntry( "BlendBalance", getProperty( desktopElem, "blending", "balance" ) );
            grp.writeEntry( "ReverseBlending",
                                    static_cast<bool>( getProperty( desktopElem, "blending", "reverse" ).toUInt() ) );
            grp.writeEntry( "Pattern", getProperty( desktopElem, "pattern", "name" ) );
            grp.writeEntry( "Wallpaper",
                                    unprocessFilePath( QLatin1String("desktop"),
                                                       getProperty( desktopElem, "wallpaper", "url" ) ) );
            grp.writeEntry( "WallpaperMode", getProperty( desktopElem, "wallpaper", "mode" ) );

            if ( common )
                break;          // stop here
        }
    }

    // 11. Screensaver
    QDomElement saverElem = m_dom.elementsByTagName( "screensaver" ).item( 0 ).toElement();

    if ( !saverElem.isNull() )
    {
        KConfigGroup grp(&_desktopConf,"ScreenSaver" );
        grp.writeEntry( "Saver", saverElem.attribute( "name" ) );
    }

    desktopConf.sync();         // TODO sync and signal only if <desktop> elem present
    // reconfigure kdesktop. kdesktop will notify all clients
    org::kde::plasma::App desktop("org.kde.plasma", "/App", QDBusConnection::sessionBus());
    desktop.initializeWallpaper();

    // FIXME Xinerama

    // 3. Icons
    QDomElement iconElem = m_dom.elementsByTagName( "icons" ).item( 0 ).toElement();
    if ( !iconElem.isNull() )
    {
        KConfigGroup iconConf(KGlobal::config(), "Icons");
        iconConf.writeEntry( "Theme", iconElem.attribute( "name", "crystalsvg" ), KConfig::Persistent|KConfig::Global);

        QDomNodeList iconList = iconElem.childNodes();
        for ( int i = 0; i < iconList.count(); i++ )
        {
            QDomElement iconSubElem = iconList.item( i ).toElement();
            QString object = iconSubElem.attribute( "object" );
            if ( object == "desktop" )
                iconConf=KConfigGroup(KGlobal::config(), "DesktopIcons" );
            else if ( object == "mainToolbar" )
                iconConf=KConfigGroup(KGlobal::config(),"MainToolbarIcons" );
            else if ( object == "panel" )
                iconConf=KConfigGroup(KGlobal::config(),"PanelIcons" );
            else if ( object == "small" )
                iconConf=KConfigGroup(KGlobal::config(),"SmallIcons" );
            else if ( object == "toolbar" )
                iconConf=KConfigGroup(KGlobal::config(),"ToolbarIcons" );

            QString iconName = iconSubElem.tagName();
            if ( iconName.contains( "Color" ) )
            {
                QColor iconColor = QColor( iconSubElem.attribute( "rgb" ) );
                iconConf.writeEntry( iconName, iconColor, KConfig::Persistent|KConfig::Global);
            }
            else if ( iconName.contains( "Value" ) || iconName == "Size" )
                iconConf.writeEntry( iconName, iconSubElem.attribute( "value" ).toUInt(), KConfig::Persistent|KConfig::Global);
            else if ( iconName.contains( "Effect" ) )
                iconConf.writeEntry( iconName, iconSubElem.attribute( "name" ), KConfig::Persistent|KConfig::Global);
            else
                iconConf.writeEntry( iconName, static_cast<bool>( iconSubElem.attribute( "value" ).toUInt() ), KConfig::Persistent|KConfig::Global);
        }
        iconConf.sync();

        for ( int i = 0; i < KIconLoader::LastGroup; i++ )
            KGlobalSettings::self()->emitChange( KGlobalSettings::IconChanged, i );
        KBuildSycocaProgressDialog::rebuildKSycoca( m_parent );
    }

    // 4. Sounds
    QDomElement soundsElem = m_dom.elementsByTagName( "sounds" ).item( 0 ).toElement();
    if ( !soundsElem.isNull() )
    {
        KConfigGroup soundGroup(KSharedConfig::openConfig("knotify.eventsrc"), "");
        KConfigGroup kwinSoundGroup(KSharedConfig::openConfig("kwin.eventsrc"), "");
        QDomNodeList eventList = soundsElem.elementsByTagName( "event" );
        for ( int i = 0; i < eventList.count(); i++ )
        {
            QDomElement eventElem = eventList.item( i ).toElement();
            QString object = eventElem.attribute( "object" );

            if ( object == "global" )
            {
                KConfigGroup grp(KSharedConfig::openConfig("knotify.eventsrc"),eventElem.attribute("name"));
                grp.writeEntry( "soundfile", unprocessFilePath( QLatin1String("sounds"), eventElem.attribute( "url" ) ) );
                grp.writeEntry( "presentation", soundGroup.readEntry( "presentation" ,0) | 1 );
            }
            else if ( object == "kwin" )
            {
                kwinSoundGroup=KConfigGroup(KSharedConfig::openConfig("kwin.eventsrc"),eventElem.attribute("name"));
                kwinSoundGroup.writeEntry( "soundfile", unprocessFilePath( QLatin1String("sounds"), eventElem.attribute( "url" ) ) );
                kwinSoundGroup.writeEntry( "presentation", soundGroup.readEntry( "presentation",0 ) | 1 );
            }
        }

        soundGroup.sync();
        kwinSoundGroup.sync();

        QDBusInterface knotify("org.kde.knotify", "/Notify", "org.kde.KNotify");
        if ( knotify.isValid() )
            knotify.call( "reconfigure" );

        // TODO signal kwin sounds change?
    }

    // 5. Colors
    QDomElement colorsElem = m_dom.elementsByTagName( "colors" ).item( 0 ).toElement();

    if ( !colorsElem.isNull() )
    {
        QDomNodeList colorList = colorsElem.childNodes();
        KConfigGroup colorGroup(KGlobal::config(), "");

        QString sCurrentScheme = KStandardDirs::locateLocal("data", "kdisplay/color-schemes/thememgr.kcsrc");
        KConfigGroup colorScheme(KSharedConfig::openConfig( sCurrentScheme, KConfig::SimpleConfig), "Color Scheme" );

        for ( int i = 0; i < colorList.count(); i++ )
        {
            QDomElement colorElem = colorList.item( i ).toElement();
            QString object = colorElem.attribute( "object" );
            if ( object == "global" )
                colorGroup=KConfigGroup(KGlobal::config(),"General");
            else if ( object == "kwin" )
                colorGroup=KConfigGroup(KGlobal::config(),"WM");

            QString colName = colorElem.tagName();
            QColor curColor = QColor( colorElem.attribute( "rgb" ) );
            colorGroup.writeEntry( colName, curColor, KConfig::Persistent|KConfig::Global); // kdeglobals
            colorScheme.writeEntry( colName, curColor ); // thememgr.kcsrc
        }

        colorGroup=KConfigGroup(KGlobal::config(),"KDE");
        colorGroup.writeEntry( "colorScheme", "thememgr.kcsrc", KConfig::Persistent|KConfig::Global);
        colorGroup.writeEntry( "contrast", colorsElem.attribute( "contrast", "7" ), KConfig::Persistent|KConfig::Global);
        colorScheme.writeEntry( "contrast", colorsElem.attribute( "contrast", "7" ) );
        colorGroup.sync();

        KGlobalSettings::self()->emitChange( KGlobalSettings::PaletteChanged );
    }

    // 6.Cursors
    QDomElement cursorsElem = m_dom.elementsByTagName( "cursors" ).item( 0 ).toElement();

    if ( !cursorsElem.isNull() )
    {
        KConfig _mouseConf( "kcminputrc" );
        KConfigGroup mouseConf(&_mouseConf, "Mouse" );
        mouseConf.writeEntry( "cursorTheme", cursorsElem.attribute( "name" ));
        // FIXME is there a way to notify KDE of cursor changes?
    }

    // 7. KWin
    QDomElement wmElem = m_dom.elementsByTagName( "wm" ).item( 0 ).toElement();

    if ( !wmElem.isNull() )
    {
        KConfig _kwinConf( "kwinrc" );
        KConfigGroup kwinConf(&_kwinConf, "Style" );
        QString type = wmElem.attribute( "type" );
        if ( type == "builtin" )
            kwinConf.writeEntry( "PluginLib", wmElem.attribute( "name" ) );
        //else // TODO support custom themes
        QDomNodeList buttons = wmElem.elementsByTagName ("buttons");
        if ( buttons.count() > 0 )
        {
            kwinConf.writeEntry( "CustomButtonPositions", true );
            kwinConf.writeEntry( "ButtonsOnLeft", getProperty( wmElem, "buttons", "left" ) );
            kwinConf.writeEntry( "ButtonsOnRight", getProperty( wmElem, "buttons", "right" ) );
        }
        else
        {
            kwinConf.writeEntry( "CustomButtonPositions", false );
        }
        kwinConf.writeEntry( "BorderSize", getProperty( wmElem, "border", "size" ) );

        kwinConf.sync();
        org::kde::KWin kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
        kwin.reconfigure();
    }

    // 8. Konqueror
    QDomElement konqElem = m_dom.elementsByTagName( "konqueror" ).item( 0 ).toElement();

    if ( !konqElem.isNull() )
    {
        KConfig _konqConf( "konquerorrc" );
        KConfigGroup konqConf(&_konqConf, "Settings" );
        konqConf.writeEntry( "BgImage", unprocessFilePath( QLatin1String("konqueror"), getProperty( konqElem, "wallpaper", "url" ) ) );
        konqConf.writeEntry( "BgColor", QColor( getProperty( konqElem, "bgcolor", "rgb" ) ) );

        konqConf.sync();

        QDBusMessage message =
            QDBusMessage::createSignal("/KonqMain", "org.kde.Konqueror.Main", "reparseConfiguration");
        QDBusConnection::sessionBus().send(message);
    }
#if 0
    // 9. Kicker
    QDomElement panelElem = m_dom.elementsByTagName( "panel" ).item( 0 ).toElement();

    if ( !panelElem.isNull() )
    {
        KConfig _kickerConf( "kickerrc" );
        KConfigGroup kickerConf(&_kickerConf, "General" );
        QString kickerBgUrl = getProperty( panelElem, "background", "url" );
        if ( !kickerBgUrl.isEmpty() )
        {
            kickerConf.writeEntry( "UseBackgroundTheme", true );
            kickerConf.writeEntry( "BackgroundTheme", unprocessFilePath( QLatin1String("panel"), kickerBgUrl ) );
            kickerConf.writeEntry( "ColorizeBackground",
                                   static_cast<bool>( getProperty( panelElem, "background", "colorize" ).toUInt() ) );
        }
        kickerConf.writeEntry( "Transparent",
                               static_cast<bool>( getProperty( panelElem, "transparent", "value" ).toUInt() ) );

        kickerConf.writeEntry( "Position", static_cast<int> (getProperty( panelElem, "position", "value" ).toUInt() ));

        kickerConf.writeEntry( "ShowLeftHideButton", static_cast<bool>( getProperty( panelElem, "showlefthidebutton", "value").toInt()));

        kickerConf.writeEntry( "ShowRightHideButton", static_cast<bool>( getProperty( panelElem, "showrighthidebutton", "value").toInt()));

        kickerConf.sync();
        QDBusInterface kicker( "org.kde.kicker", "kicker");
        kicker.call("configure");
    }
#endif
    // 10. Widget style
    QDomElement widgetsElem = m_dom.elementsByTagName( "widgets" ).item( 0 ).toElement();

    if ( !widgetsElem.isNull() )
    {
        KConfigGroup widgetConf(KGlobal::config(), "General");
        widgetConf.writeEntry( "widgetStyle", widgetsElem.attribute( "name" ), KConfig::Persistent|KConfig::Global);
        widgetConf.sync();
        KGlobalSettings::self()->emitChange( KGlobalSettings::StyleChanged );
    }

    // 12. Fonts
    QDomElement fontsElem = m_dom.elementsByTagName( "fonts" ).item( 0 ).toElement();
    if ( !fontsElem.isNull() )
    {
        KConfigGroup fontsGroup(KGlobal::config(), "");
        KConfigGroup kde1xGroup(KSharedConfig::openConfig(QDir::homePath() + "/.kderc"),
                                "General");

        QDomNodeList fontList = fontsElem.childNodes();
        for ( int i = 0; i < fontList.count(); i++ )
        {
            QDomElement fontElem = fontList.item( i ).toElement();
            QString fontName  = fontElem.tagName();
            QString fontValue = fontElem.attribute( "value" );
            QString fontObject = fontElem.attribute( "object" );

            if ( fontObject == "FMSettings" ) {
                desktopConf.changeGroup( fontObject );
                desktopConf.writeEntry( fontName, fontValue, KConfig::Persistent|KConfig::Global);
                desktopConf.sync();
            }
            else {
                fontsGroup.changeGroup(fontObject);
                fontsGroup.writeEntry( fontName, fontValue, KConfig::Persistent|KConfig::Global);
            }
            kde1xGroup.writeEntry( fontName, fontValue, KConfig::Persistent|KConfig::Global);
        }

        fontsGroup.sync();
        kde1xGroup.sync();
        KGlobalSettings::self()->emitChange( KGlobalSettings::FontChanged );
    }

}

bool KTheme::remove( const QString & name )
{
    kDebug() << "Going to remove theme: " << name;
    return KIO::NetAccess::del( KUrl(KGlobal::dirs()->saveLocation( "themes", name + '/' )), 0L );
}

void KTheme::setProperty( const QString & name, const QString & value, QDomElement & parent )
{
    QDomElement tmp = m_dom.createElement( name );
    tmp.setAttribute( "value", value );
    parent.appendChild( tmp );
}

QString KTheme::getProperty( const QString & name ) const
{
    QDomNodeList _list = m_dom.elementsByTagName( name );
    if ( _list.count() != 0 )
        return _list.item( 0 ).toElement().attribute( "value" );
    else
    {
        kWarning() << "Found no such property: " << name ;
        return QString();
    }
}

QString KTheme::getProperty( QDomElement & parent, const QString & tag,
                             const QString & attr ) const
{
    QDomNodeList _list = parent.elementsByTagName( tag );

    if ( _list.count() != 0 )
        return _list.item( 0 ).toElement().attribute( attr );
    else
    {
        kWarning() << QString( "No such property found: %1->%2->%3" )
            .arg( parent.tagName() ).arg( tag ).arg( attr ) << endl;
        return QString();
    }
}

void KTheme::createIconElems( const KConfigGroup & group, const QString & object,
                              QDomElement & parent )
{
    QStringList elemNames;
    elemNames << "Animated" << "DoublePixels" << "Size"
              << "ActiveColor" << "ActiveColor2" << "ActiveEffect"
              << "ActiveSemiTransparent" << "ActiveValue"
              << "DefaultColor" << "DefaultColor2" << "DefaultEffect"
              << "DefaultSemiTransparent" << "DefaultValue"
              << "DisabledColor" << "DisabledColor2" << "DisabledEffect"
              << "DisabledSemiTransparent" << "DisabledValue";
    for ( QStringList::ConstIterator it = elemNames.begin(); it != elemNames.end(); ++it ) {
        if ( (*it).contains( "Color" ) )
            createColorElem( *it, object, parent, group );
        else
        {
            QDomElement tmpCol = m_dom.createElement( *it );
            tmpCol.setAttribute( "object", object );

            if ( (*it).contains( "Value" ) || *it == "Size" )
                tmpCol.setAttribute( "value", group.readEntry( *it, 1 ) );
        else if ( (*it).contains( "DisabledEffect" ) )
        tmpCol.setAttribute( "name", group.readEntry( *it, QString("togray") ) );
            else if ( (*it).contains( "Effect" ) )
                tmpCol.setAttribute( "name", group.readEntry( *it, QString("none") ) );
            else
                tmpCol.setAttribute( "value", group.readEntry( *it, false ) );
            parent.appendChild( tmpCol );
        }
    }
}

void KTheme::createColorElem( const QString & name, const QString & object,
                              QDomElement & parent, const KConfigGroup & group )
{
    QColor color = group.readEntry( name,QColor() );
    if ( color.isValid() )
    {
        QDomElement tmpCol = m_dom.createElement( name );
        tmpCol.setAttribute( "rgb", color.name() );
        tmpCol.setAttribute( "object", object );
        parent.appendChild( tmpCol );
    }
}

void KTheme::createSoundList( const QStringList & events, const QString & object,
                              QDomElement & parent, KConfig * cfg )
{
    for ( QStringList::ConstIterator it = events.begin(); it != events.end(); ++it )
    {
        KConfigGroup group(cfg, *it);
        if ( group.exists() )
        {
            QString soundURL = group.readPathEntry( "soundfile", QString() );
            int pres = group.readEntry( "presentation", 0 );
            if ( !soundURL.isEmpty() && ( ( pres & 1 ) == 1 ) )
            {
                QDomElement eventElem = m_dom.createElement( "event" );
                eventElem.setAttribute( "object", object );
                eventElem.setAttribute( "name", group.name() );
                eventElem.setAttribute( "url", processFilePath( "sounds", soundURL ) );
                parent.appendChild( eventElem );
            }
        }
    }
}

QString KTheme::processFilePath( const QString & section, const QString & path )
{
    QFileInfo fi( path );

    if ( fi.isRelative() )
        fi.setFile( findResource( section, path ) );

    kDebug() << "Processing file: " << fi.absoluteFilePath() << ", " << fi.fileName();

    if ( section == "desktop" )
    {
        if ( copyFile( fi.absoluteFilePath(), m_kgd->saveLocation( "themes", m_name + "/wallpapers/desktop/" ) + '/' + fi.fileName() ) )
            return "theme:/wallpapers/desktop/" + fi.fileName();
    }
    else if ( section == "sounds" )
    {
        if ( copyFile( fi.absoluteFilePath(), m_kgd->saveLocation( "themes", m_name + "/sounds/" ) + '/' + fi.fileName() ) )
            return "theme:/sounds/" + fi.fileName();
    }
    else if ( section == "konqueror" )
    {
        if ( copyFile( fi.absoluteFilePath(), m_kgd->saveLocation( "themes", m_name + "/wallpapers/konqueror/" ) + '/' + fi.fileName() ) )
            return "theme:/wallpapers/konqueror/" + fi.fileName();
    }
    else if ( section == "panel" )
    {
        if ( copyFile( fi.absoluteFilePath(), m_kgd->saveLocation( "themes", m_name + "/wallpapers/panel/" ) + '/' + fi.fileName() ) )
            return "theme:/wallpapers/panel/" + fi.fileName();
    }
    else
        kWarning() << "Unsupported theme resource type" ;

    return QString();       // an error occurred or the resource doesn't exist
}

QString KTheme::unprocessFilePath( QString section, QString path )
{
    if ( path.startsWith( "theme:/" ) )
        return path.replace( QRegExp( "^theme:/" ), m_kgd->findResourceDir( "themes", m_name + '/' + m_name + ".xml") + m_name + '/' );

    if ( QFile::exists( path ) )
        return path;
    else  // try to find it in the system
        return findResource( section, path );
}

void KTheme::setAuthor( const QString & author )
{
    setProperty( "author", author, m_general );
}

void KTheme::setEmail( const QString & email )
{
    setProperty( "email", email, m_general );
}

void KTheme::setHomepage( const QString & homepage )
{
    setProperty( "homepage", homepage, m_general );
}

void KTheme::setComment( const QString & comment )
{
    setProperty( "comment", comment, m_general );
}

void KTheme::setVersion( const QString & version )
{
    setProperty( "version", version, m_general );
}

void KTheme::addPreview()
{
    QString file = m_kgd->saveLocation( "themes", m_name + '/' ) + m_name + ".preview.png";
    kDebug() << "Adding preview: " << file;
    QPixmap snapshot = QPixmap::grabWindow( QX11Info::appRootWindow() );
    snapshot.save( file, "PNG" );
}

bool KTheme::copyFile( const QString & from, const QString & to )
{
    // we overwrite b/c of restoring the "original" theme
    return KIO::file_copy( KUrl(from), KUrl(to), -1, KIO::HideProgressInfo | KIO::Overwrite /*overwrite*/ );
}

QString KTheme::findResource( const QString & section, const QString & path )
{
    if ( section == "desktop" )
        return m_kgd->findResource( "wallpaper", path );
    else if ( section == "sounds" )
        return m_kgd->findResource( "sound", path );
    else if ( section == "konqueror" )
        return m_kgd->findResource( "data", "konqueror/tiles/" + path );
    else if ( section == "panel" )
        return m_kgd->findResource( "data", "kicker/wallpapers/" + path );
    else
    {
        kWarning() << "Requested unknown resource: " << section ;
        return QString();
    }
}
