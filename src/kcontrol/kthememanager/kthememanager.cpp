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

#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>

//Added by qt3to4:
#include <QDragEnterEvent>
#include <QVBoxLayout>
#include <QDropEvent>
#include <QBoxLayout>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kdialog.h>
#include <kemailsettings.h>
#include <kfiledialog.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <k3listview.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kservicegroup.h>
#include <kstandarddirs.h>
#include <k3urldrag.h>
#include <krun.h>

#include "kthememanager.h"
#include "knewthemedlg.h"
#include <config-workspace.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>


K_PLUGIN_FACTORY(kthememanagerFactory, registerPlugin<kthememanager>();)
K_EXPORT_PLUGIN(kthememanagerFactory("kthememanager"))


kthememanager::kthememanager( QWidget *parent, const QVariantList & )
    : KCModule( kthememanagerFactory::componentData(), parent ), m_theme( 0 ), m_origTheme( 0 )
{

    KAboutData *about = new KAboutData("kthememanager", 0, ki18n("KDE Theme Manager"),
                                       "0.4", ki18n("This control module handles installing, removing and "
                                                        "creating visual KDE themes."),
                                       KAboutData::License_GPL, ki18n("(c) 2003, 2004 Lukáš Tinkl"), KLocalizedString(),
                                       "http://developer.kde.org/~lukas/kthememanager");
    setAboutData( about );

    setQuickHelp( i18n("This control module handles installing, removing and "
                "creating visual KDE themes."));

    setButtons( KCModule::Default|KCModule::Apply );

    setAcceptDrops( true );
    init();

    QBoxLayout *top = new QVBoxLayout(this);
    top->setMargin(0);
    top->setSpacing(KDialog::spacingHint());

    dlg = new KThemeDlg(this);
    top->addWidget( dlg );

    dlg->lvThemes->setColumnWidthMode( 0, Q3ListView::Maximum );

    connect( ( QObject * )dlg->btnInstall, SIGNAL( clicked() ),
             this, SLOT( slotInstallTheme() ) );

    connect( ( QObject * )dlg->btnRemove, SIGNAL( clicked() ),
             this, SLOT( slotRemoveTheme() ) );

    connect( ( QObject * )dlg->btnCreate, SIGNAL( clicked() ),
             this, SLOT( slotCreateTheme() ) );

    connect( ( QObject * )dlg->lvThemes, SIGNAL( clicked( Q3ListViewItem * ) ),
             this, SLOT( slotThemeChanged( Q3ListViewItem * ) ) );

    connect( this, SIGNAL( filesDropped( const KUrl::List& ) ),
             this, SLOT( updateButton() ) );

    connect( ( QObject * )dlg->lvThemes, SIGNAL( clicked( Q3ListViewItem * ) ),
             this, SLOT( updateButton() ) );

    connect( dlg->lbGet, SIGNAL(leftClickedUrl(QString)),SLOT(startKonqui(QString) ) );
    connect( dlg->btnBackground, SIGNAL(clicked()),SLOT(startBackground()) );
    connect( dlg->btnColors, SIGNAL(clicked()),SLOT(startColors()) );
    connect( dlg->btnStyle, SIGNAL(clicked()),SLOT(startStyle()) );
    connect( dlg->btnIcons, SIGNAL(clicked()),SLOT(startIcons()) );
    connect( dlg->btnFonts, SIGNAL(clicked()),SLOT(startFonts()) );
    connect( dlg->btnSaver, SIGNAL(clicked()),SLOT(startSaver()) );

    m_origTheme = new KTheme( this, true ); // stores the defaults to get back to
    m_origTheme->setName( ORIGINAL_THEME );
    m_origTheme->createYourself();

    queryLNFModules();
    updateButton();
}

kthememanager::~kthememanager()
{
    delete m_theme;
    delete m_origTheme;
}


void kthememanager::startKonqui( const QString & url )
{
    (void) new KRun(url,this);
}

void kthememanager::startBackground()
{
    KRun::runCommand("kcmshell4 background", this);
}

void kthememanager::startColors()
{
    KRun::runCommand("kcmshell4 colors", this);
}

void kthememanager::startStyle()
{
    KRun::runCommand("kcmshell4 style", this);
}

void kthememanager::startIcons()
{
    KRun::runCommand("kcmshell4 icons", this);
}

void kthememanager::startFonts()
{
   KRun::runCommand("kcmshell4 fonts", this);
}

void kthememanager::startSaver()
{
    KRun::runCommand("kcmshell4 screensaver", this);
}

void kthememanager::init()
{
    KGlobal::dirs()->addResourceType( "themes", "data", "kthememanager/themes/" );
}

void kthememanager::updateButton()
{
    Q3ListViewItem * cur = dlg->lvThemes->currentItem();
    bool enable = (cur != 0);
    if (enable) {
         enable = QFile(KGlobal::dirs()->saveLocation( "themes",  cur->text( 0 ) + '/'+ cur->text( 0 )+ ".xml" ,false/*don't create dir*/ )).exists();
    }
    dlg->btnRemove->setEnabled(enable);

}

void kthememanager::load()
{
    listThemes();

    // Load the current theme name
    KConfig _conf("kcmthememanagerrc", KConfig::NoGlobals);
    KConfigGroup conf(&_conf, "General" );
    QString themeName = conf.readEntry( "CurrentTheme" );
    Q3ListViewItem * cur =  dlg->lvThemes->findItem( themeName, 0 );
    if ( cur )
    {
        dlg->lvThemes->setSelected( cur, true );
        dlg->lvThemes->ensureItemVisible( cur );
        slotThemeChanged( cur );
    }
}

void kthememanager::defaults()
{
    if ( m_origTheme )
        m_origTheme->apply();
}

void kthememanager::save()
{
    Q3ListViewItem * cur = dlg->lvThemes->currentItem();

    if ( cur )
    {
        QString themeName = cur->text( 0 );

        m_theme = new KTheme( this, KGlobal::dirs()->findResource( "themes", themeName + '/' + themeName + ".xml") );
        m_theme->apply();

        // Save the current theme name
        KConfig _conf("kcmthememanagerrc", KConfig::NoGlobals);
        KConfigGroup conf(&_conf, "General" );
        conf.writeEntry( "CurrentTheme", themeName );
        conf.sync();

        delete m_theme;
        m_theme = 0;

    }
}

void kthememanager::listThemes()
{
    dlg->lvThemes->clear();
    dlg->lbPreview->setPixmap( QPixmap() );

    QStringList themes = KGlobal::dirs()->findAllResources( "themes", "*.xml", KStandardDirs::Recursive );

    QStringList::const_iterator it;

    for ( it = themes.begin(); it != themes.end(); ++it )
    {
        KTheme theme( this, ( *it ) );
        QString name = theme.name();
        if ( name != ORIGINAL_THEME ) // skip the "original" theme
            ( void ) new Q3ListViewItem( dlg->lvThemes, name, theme.comment() );
    }

    kDebug() << "Available themes: " << themes;
}

float kthememanager::getThemeVersion( const QString & themeName )
{
    QStringList themes = KGlobal::dirs()->findAllResources( "themes", "*.xml", KStandardDirs::Recursive );

    QStringList::const_iterator it;

    for ( it = themes.begin(); it != themes.end(); ++it )
    {
        KTheme theme( 0L, ( *it ) );
        QString name = theme.name();
        bool ok = false;
        float version = theme.version().toFloat( &ok );
        if ( name == themeName && ok )
            return version;
    }

    return -1;
}

void kthememanager::slotInstallTheme()
{
    addNewTheme( KFileDialog::getOpenUrl( KUrl::fromPath(":themes"), "*.kth|" + i18n("Theme Files"), this,
                                          i18n( "Select Theme File" ) ) );
}

void kthememanager::addNewTheme( const KUrl & url )
{
    if ( url.isValid() )
    {
        QString themeName = QFileInfo( url.fileName() ).baseName();
        if ( getThemeVersion( themeName ) != -1 ) // theme exists already
        {
            KTheme::remove( themeName  ); // remove first
        }

        m_theme = new KTheme(this);
        if ( m_theme->load( url ) )
        {
            listThemes();
            emit changed( true );
        }

        delete m_theme;
        m_theme = 0;
        updateButton();
    }
}

void kthememanager::slotRemoveTheme()
{
    // get the selected item from the listview
    Q3ListViewItem * cur = dlg->lvThemes->currentItem();
    // ask and remove it
    if ( cur )
    {
        QString themeName = cur->text( 0 );
        if ( KMessageBox::warningContinueCancel( this, "<qt>" + i18n( "Do you really want to remove the theme <b>%1</b>?", themeName ),
                                         i18n( "Remove Theme" ),KStandardGuiItem::remove() )
             == KMessageBox::Continue )
        {
            KTheme::remove( themeName );
            listThemes();
        }
        updateButton();
    }
}

bool kthememanager::themeExist(const QString &_themeName)
{
    return ( dlg->lvThemes->findItem( _themeName, 0 )!=0 );
}

void kthememanager::slotCreateTheme()
{
    KNewThemeDlg dlg( this );

    KEMailSettings es;
    es.setProfile( es.defaultProfileName() );

    dlg.setName( i18n( "My Theme" ) );
    dlg.setAuthor( es.getSetting( KEMailSettings::RealName ) ) ;
    dlg.setEmail( es.getSetting( KEMailSettings::EmailAddress ) );
    dlg.setVersion( "0.1" );

    if ( dlg.exec() == QDialog::Accepted )
    {

        QString themeName = dlg.getName();
        if ( themeExist(themeName) )
        {
            KMessageBox::information( this, i18n( "Theme %1 already exists.", themeName ) );
        }
        else
        {
            if ( getThemeVersion( themeName ) != -1 ) // remove the installed theme first
            {
                KTheme::remove( themeName );
            }
            m_theme = new KTheme( this, true );
            m_theme->setName( dlg.getName() );
            m_theme->setAuthor( dlg.getAuthor() );
            m_theme->setEmail( dlg.getEmail() );
            m_theme->setHomepage( dlg.getHomepage() );
            m_theme->setComment( dlg.getComment().replace( "\n", "" ) );
            m_theme->setVersion( dlg.getVersion() );

            QString result = m_theme->createYourself( true );
            m_theme->addPreview();

            if ( !result.isEmpty() )
                KMessageBox::information( this, i18nc( "%1 is theme archive name", "Your theme has been successfully created in %1.", result ),
                                          i18n( "Theme Created" ), "theme_created_ok" );
            else
                KMessageBox::error( this, i18n( "An error occurred while creating your theme." ),
                                    i18n( "Theme Not Created" ) );
            delete m_theme;
            m_theme = 0;

            listThemes();
        }
    }
}

void kthememanager::slotThemeChanged( Q3ListViewItem * item )
{
    if ( item )
    {
        QString themeName = item->text(0);
        kDebug() << "Activated theme: " << themeName ;

        QString themeDir = KGlobal::dirs()->findResourceDir( "themes", themeName + '/' + themeName + ".xml") + themeName + '/';

        QString pixFile = themeDir + themeName + ".preview.png";

        if ( QFile::exists( pixFile ) )
        {
            updatePreview( pixFile );
        }
        else
        {
            dlg->lbPreview->setPixmap( QPixmap() );
            dlg->lbPreview->setText( i18n( "This theme does not contain a preview." ) );
        }

        KTheme theme( this, themeDir + themeName + ".xml" );
        dlg->lbPreview->setToolTip( "<qt>" + i18n( "Author: %1<br />Email: %2<br />Version: %3<br />Homepage: %4" ,
                         theme.author(), theme.email() ,
                         theme.version(), theme.homepage() ) + "</qt>");

        emit changed( true );
    }
}

void kthememanager::dragEnterEvent( QDragEnterEvent * ev )
{
    ev->setAccepted( K3URLDrag::canDecode( ev ) );
}

void kthememanager::dropEvent( QDropEvent * ev )
{
    KUrl::List urls;
    if ( K3URLDrag::decode( ev, urls ) )
    {
        emit filesDropped( urls );
    }
}

void kthememanager::slotFilesDropped( const KUrl::List & urls )
{
    for ( KUrl::List::ConstIterator it = urls.begin(); it != urls.end(); ++it )
        addNewTheme( *it );
}

void kthememanager::queryLNFModules()
{
    /*KServiceGroup::Ptr settings = KServiceGroup::group( "Settings/LookNFeel/" );
    if ( !settings || !settings->isValid() )
        return;

    KServiceGroup::List list = settings->entries();

    // Iterate over all entries in the group
    for( KServiceGroup::List::ConstIterator it = list.begin();
         it != list.end(); it++ )
    {
        KSycocaEntry *p = ( *it );
        if ( p->isType( KST_KService ) )
        {
            KService *s = static_cast<KService *>( p );
            ( void ) new KThemeDetailsItem( dlg->lvDetails, s->name(), s->pixmap( KIconLoader::Desktop ), s->exec() );
        }
    }

    dlg->lvDetails->sort();*/

    // For now use a static list
    dlg->btnBackground->setIcon( KIcon( "preferences-desktop-wallpaper" ) );
    dlg->btnColors->setIcon( KIcon( "preferences-desktop-color" ) );
    dlg->btnStyle->setIcon( KIcon( "preferences-desktop-theme-style" ) );
    dlg->btnIcons->setIcon( KIcon( "preferences-desktop-icons" ) );
    dlg->btnFonts->setIcon( KIcon( "preferences-desktop-font" ) );
    dlg->btnSaver->setIcon( KIcon( "preferences-desktop-wallpaper" ) );
}

void kthememanager::updatePreview( const QString & pixFile )
{
     kDebug() << "Preview is in file: " << pixFile;
     QImage preview( pixFile, "PNG" );
     if (preview.width()>dlg->lbPreview->contentsRect().width() ||
         preview.height()>dlg->lbPreview->contentsRect().height() )
         preview = preview.scaled( dlg->lbPreview->contentsRect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
     QPixmap pix = QPixmap::fromImage( preview );
     dlg->lbPreview->setPixmap( pix );
}

#include "kthememanager.moc"
