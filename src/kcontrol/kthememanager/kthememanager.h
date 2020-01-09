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

#ifndef KTHEMEMANAGER_H
#define KTHEMEMANAGER_H

#include <kcmodule.h>
#include <krun.h>
#include <kservice.h>
#include <kurl.h>

#include "ui_kthemedlg.h"
#include "ktheme.h"

class QString;

class QStringList;

#define ORIGINAL_THEME "original" // no i18n() here!!!

/*
class K3IconViewItem;

class KThemeDetailsItem: public K3IconViewItem
{
public:
    KThemeDetailsItem( K3IconView * parent, const QString & text, const QPixmap & icon, const QString & execString )
        : K3IconViewItem( parent, text, icon ) { m_exec = execString; }
    virtual ~KThemeDetailsItem() { };

    void exec() {
        ( void ) new KRun( m_exec );
    }
private:
    QString m_exec;
};
*/


class KThemeDlg : public QWidget, public Ui::KThemeDlg
{
public:
  KThemeDlg( QWidget *parent ) : QWidget( parent ) {
    setupUi( this );
  }
};

/**
 *
 * This is the for KControl config module for installing,
 * creating and removing visual themes.
 *
 * @brief The Theme Manager config module.
 * @author Lukas Tinkl <lukas@kde.org>
 */
class kthememanager: public KCModule
{
    Q_OBJECT
public:
    kthememanager( QWidget *parent, const QVariantList &args );
    virtual ~kthememanager();

    /**
     * Called on module startup
     */
    virtual void load();
    /**
     * Called when applying the changes
     */
    virtual void save();
    /**
     * Called when the user requests the default values
     */
    virtual void defaults();

protected:
    void dragEnterEvent ( QDragEnterEvent * ev );
    void dropEvent ( QDropEvent * ev );

Q_SIGNALS:
    /**
     * Emitted when some @p urls are dropped onto the kcm
     */
    void filesDropped(const KUrl::List &urls);

private Q_SLOTS:
    /**
     * Install a theme from a tarball (*.kth)
     */
    void slotInstallTheme();

    /**
     * Remove an installed theme
     */
    void slotRemoveTheme();

    /**
     * Create a new theme
     */
    void slotCreateTheme();

    /**
     * Update the theme's info and preview
     */
    void slotThemeChanged( Q3ListViewItem * item );

    /**
     * Invoked when one drag and drops @p urls onto the kcm
     * @see signal filesDropped
     */
    void slotFilesDropped( const KUrl::List & urls );
    void updateButton();


    void startKonqui( const QString & url );
    void startBackground();
    void startColors();
    void startStyle();
    void startIcons();
    void startFonts();
    void startSaver();


private:
    /**
     * List themes available in the system and insert them into the listview.
     */
    void listThemes();

    /**
     * Performs the actual theme installation.
     */
    void addNewTheme( const KUrl & url );

    /**
     * Perform internal initialization of paths.
     */
    void init();

    /**
     * Try to find out whether a theme is installed and get its version number
     * @param themeName The theme name
     * @return The theme's version number or -1 if not installed
     */
    static float getThemeVersion( const QString & themeName );

    void queryLNFModules();

    /**
     * Updates the preview widget
     */
    void updatePreview( const QString & pixFile );
    bool themeExist(const QString &_themeName);
    KThemeDlg * dlg;

    KTheme * m_theme;
    KTheme * m_origTheme;
};

#endif
