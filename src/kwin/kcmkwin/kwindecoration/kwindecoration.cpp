/*
	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed <gallium@kde.org>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders)
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include <assert.h>
#include <QDir>
#include <QFileInfo>
#include <QLayout>

#include <QGroupBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QTabWidget>

#include <QLabel>
#include <QFile>
#include <QSlider>
//Added by qt3to4:
#include <QPixmap>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QtDBus/QtDBus>

#include <kapplication.h>
#include <kcombobox.h>
#include <kdebug.h>
#include <kdesktopfile.h>
#include <kstandarddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <kaboutdata.h>

#include "kwindecoration.h"
#include "preview.h"
#include <kdecoration_plugins_p.h>
#include <kdecorationfactory.h>
#include <kvbox.h>
#include <KPluginFactory>
#include <KPluginLoader>

// KCModule plugin interface
// =========================
K_PLUGIN_FACTORY(KWinDecoFactory,
        registerPlugin<KWinDecorationModule>();
        )
K_EXPORT_PLUGIN(KWinDecoFactory("kcmkwindecoration"))

KWinDecorationModule::KWinDecorationModule(QWidget* parent, const QVariantList &)
    : KCModule(KWinDecoFactory::componentData(), parent),
    kwinConfig(KSharedConfig::openConfig("kwinrc")),
    pluginObject(0)
{
    KConfigGroup style( kwinConfig, "Style");
    plugins = new KDecorationPreviewPlugins(kwinConfig);

	QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setMargin(0);

// Save this for later...
//	cbUseMiniWindows = new QCheckBox( i18n( "Render mini &titlebars for all windows"), checkGroup );
//	QWhatsThis::add( cbUseMiniWindows, i18n( "Note that this option is not available on all styles yet" ) );

	tabWidget = new QTabWidget( this );
	layout->addWidget( tabWidget );

	// Page 1 (General Options)
	QWidget *pluginPage = new QWidget( tabWidget );

	QVBoxLayout* pluginLayout = new QVBoxLayout(pluginPage);

	// decoration chooser
	decorationList = new KComboBox( pluginPage );
	QString whatsThis = i18n("Select the window decoration. This is the look and feel of both "
                             "the window borders and the window handle.");
	decorationList->setWhatsThis( whatsThis);
	pluginLayout->addWidget(decorationList);

	pluginSettingsGrp = new QGroupBox( i18n("Decoration Options"), pluginPage );
    QVBoxLayout *pluginSettingsLayout = new QVBoxLayout();
	pluginSettingsGrp->setFlat( true );
	pluginSettingsLayout->setMargin( 0 );
	pluginSettingsGrp->setLayout( pluginSettingsLayout );
	pluginLayout->addWidget( pluginSettingsGrp );

	pluginLayout->addStretch();

	// Border size chooser
	lBorder = new QLabel (i18n("B&order size:"), pluginSettingsGrp);
	cBorder = new QComboBox(pluginSettingsGrp);
	lBorder->setBuddy(cBorder);
	cBorder->setWhatsThis( i18n( "Use this combobox to change the border size of the decoration." ));
	lBorder->hide();
	cBorder->hide();
	QHBoxLayout *borderSizeLayout = new QHBoxLayout();
    pluginSettingsLayout->addLayout( borderSizeLayout );
	borderSizeLayout->addWidget(lBorder);
	borderSizeLayout->addWidget(cBorder);
	borderSizeLayout->addStretch();

	pluginConfigWidget = new KVBox(pluginSettingsGrp);
	pluginSettingsLayout->addWidget( pluginConfigWidget );

	// Page 2 (Button Selector)
	QWidget* buttonPage = new QWidget( tabWidget );
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonPage);

	cbShowToolTips = new QCheckBox(
			i18n("&Show window button tooltips"), buttonPage );
	cbShowToolTips->setWhatsThis(
			i18n(  "Enabling this checkbox will show window button tooltips. "
				   "If this checkbox is off, no window button tooltips will be shown."));

	cbUseCustomButtonPositions = new QCheckBox(
			i18n("Use custom titlebar button &positions"), buttonPage );
	cbUseCustomButtonPositions->setWhatsThis(
			i18n(  "The appropriate settings can be found in the \"Buttons\" Tab; "
				   "please note that this option is not available on all styles yet." ) );

	buttonLayout->addWidget( cbShowToolTips );
	buttonLayout->addWidget( cbUseCustomButtonPositions );

	// Add nifty dnd button modification widgets
	buttonPositionWidget = new ButtonPositionWidget(buttonPage, "button_position_widget");
	buttonPositionWidget->setDecorationFactory(plugins->factory() );
	QHBoxLayout* buttonControlLayout = new QHBoxLayout();
    buttonLayout->addLayout( buttonControlLayout );
	buttonControlLayout->addSpacing(20);
	buttonControlLayout->addWidget(buttonPositionWidget);
// 	buttonLayout->addStretch();

	// preview
	QVBoxLayout* previewLayout = new QVBoxLayout();
    layout->addLayout( previewLayout );
	previewLayout->setMargin( 0 );

	preview = new KDecorationPreview( this );
	previewLayout->addWidget(preview);

	preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

	// Load all installed decorations into memory
	// Set up the decoration lists and other UI settings
	findDecorations();
	createDecorationList();
	readConfig( style );

	resetPlugin( style );

	tabWidget->addTab( pluginPage, i18n("&Window Decoration") );
	tabWidget->addTab( buttonPage, i18n("&Buttons") );

	connect( buttonPositionWidget, SIGNAL(changed()), this, SLOT(slotButtonsChanged()) ); // update preview etc.
	connect( buttonPositionWidget, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) ); // emit changed()...
	connect( decorationList, SIGNAL(activated(const QString&)), SLOT(slotSelectionChanged()) );
	connect( decorationList, SIGNAL(activated(const QString&)),
								SLOT(slotChangeDecoration(const QString&)) );
	connect( cbUseCustomButtonPositions, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
	connect(cbUseCustomButtonPositions, SIGNAL(toggled(bool)), buttonPositionWidget, SLOT(setEnabled(bool)));
	connect(cbUseCustomButtonPositions, SIGNAL(toggled(bool)), this, SLOT(slotButtonsChanged()) );
	connect( cbShowToolTips, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
	connect( cBorder, SIGNAL( activated( int )), SLOT( slotBorderChanged( int )));
//	connect( cbUseMiniWindows, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );

	KAboutData *about =
		new KAboutData(I18N_NOOP("kcmkwindecoration"), 0,
				ki18n("Window Decoration Control Module"),
				0, KLocalizedString(), KAboutData::License_GPL,
				ki18n("(c) 2001 Karol Szwed"));
	about->addAuthor(ki18n("Karol Szwed"), KLocalizedString(), "gallium@kde.org");
	setAboutData(about);
}


KWinDecorationModule::~KWinDecorationModule()
{
        delete preview; // needs to be destroyed before plugins
        delete plugins;
}


// Find all theme desktop files in all 'data' dirs owned by kwin.
// And insert these into a DecorationInfo structure
void KWinDecorationModule::findDecorations()
{
	const QStringList dirList = KGlobal::dirs()->findDirs("data", "kwin");
	QStringList::ConstIterator it;

	for (it = dirList.constBegin(); it != dirList.constEnd(); ++it)
	{
		QDir d(*it);
		if (d.exists())
			foreach (const QFileInfo& fi, d.entryInfoList())
			{
				QString filename(fi.absoluteFilePath());
				if (KDesktopFile::isDesktopFile(filename))
				{
					KDesktopFile desktopFile(filename);
					QString libName = desktopFile.desktopGroup().readEntry("X-KDE-Library");

					if (!libName.isEmpty() && libName.startsWith( "kwin3_" ))
					{
						DecorationInfo di;
						di.name = desktopFile.readName();
						di.libraryName = libName;
						decorations.append( di );
					}
				}
			}
	}
}


// Fills the decorationList with a list of available kwin decorations
void KWinDecorationModule::createDecorationList()
{
	QList<DecorationInfo>::ConstIterator it;

	// Sync with kwin hardcoded KDE2 style which has no desktop item
    QStringList decorationNames;
//	decorationNames.append( i18n("KDE 2") );
	for (it = decorations.constBegin(); it != decorations.constEnd(); ++it)
	{
		decorationNames.append((*it).name);
	}
	decorationNames.sort();
    decorationList->addItems(decorationNames);
}


// Reset the decoration plugin to what the user just selected
void KWinDecorationModule::slotChangeDecoration( const QString & text)
{
	KConfig _kwinConfig( "kwinrc" );
	KConfigGroup kwinConfig(&_kwinConfig, "Style");

	// Let the user see config options for the currently selected decoration
	resetPlugin( kwinConfig, text );
}


// This is the selection handler setting
void KWinDecorationModule::slotSelectionChanged()
{
	emit KCModule::changed(true);
}

static const char* const border_names[ KDecorationDefines::BordersCount ] =
    {
    I18N_NOOP( "Tiny" ),
    I18N_NOOP( "Normal" ),
    I18N_NOOP( "Large" ),
    I18N_NOOP( "Very Large" ),
    I18N_NOOP( "Huge" ),
    I18N_NOOP( "Very Huge" ),
    I18N_NOOP( "Oversized" )
    };

int KWinDecorationModule::borderSizeToIndex( BorderSize size, QList< BorderSize > sizes )
{
        int pos = 0;
        for( QList< BorderSize >::ConstIterator it = sizes.constBegin();
             it != sizes.constEnd();
             ++it, ++pos )
            if( size <= *it )
                break;
        return pos;
}

KDecorationDefines::BorderSize KWinDecorationModule::indexToBorderSize( int index,
    QList< BorderSize > sizes )
{
        QList< BorderSize >::ConstIterator it = sizes.constBegin();
        for(;
             it != sizes.constEnd();
             ++it, --index )
            if( index == 0 )
                break;
        return *it;
}

void KWinDecorationModule::slotBorderChanged( int size )
{
        if( lBorder->isHidden())
            return;
        emit KCModule::changed( true );
        QList< BorderSize > sizes;
        if( plugins->factory() != NULL )
            sizes = plugins->factory()->borderSizes();
        assert( sizes.count() >= 2 );
        border_size = indexToBorderSize( size, sizes );

	// update preview
	preview->setTempBorderSize(plugins, border_size);
}

void KWinDecorationModule::slotButtonsChanged()
{
	// update preview
	preview->setTempButtons(plugins, cbUseCustomButtonPositions->isChecked(), buttonPositionWidget->buttonsLeft(), buttonPositionWidget->buttonsRight() );
}

QString KWinDecorationModule::decorationName( QString& libName )
{
	QString decoName;

	QList<DecorationInfo>::Iterator it;
	for( it = decorations.begin(); it != decorations.end(); ++it )
		if ( (*it).libraryName == libName )
		{
			decoName = (*it).name;
			break;
		}

	return decoName;
}


QString KWinDecorationModule::decorationLibName( const QString& name )
{
	QString libName;

	// Find the corresponding library name to that of
	// the current plugin name
	QList<DecorationInfo>::Iterator it;
	for( it = decorations.begin(); it != decorations.end(); ++it )
		if ( (*it).name == name )
		{
			libName = (*it).libraryName;
			break;
		}

//	if (libName.isEmpty())
//		libName = "kwin_default";	// KDE 2

	return libName;
}


// Loads/unloads and inserts the decoration config plugin into the
// pluginConfigWidget, allowing for dynamic configuration of decorations
void KWinDecorationModule::resetPlugin( KConfigGroup& conf, const QString& currentDecoName )
{
	// Config names are "kwin_icewm_config"
	// for "kwin3_icewm" kwin client

	QString oldName = styleToConfigLib( oldLibraryName );

	QString currentName;
	if (!currentDecoName.isEmpty())
		currentName = decorationLibName( currentDecoName ); // Use what the user selected
	else
		currentName = currentLibraryName; // Use what was read from readConfig()

        if( plugins->loadPlugin( currentName )
            && preview->recreateDecoration( plugins ))
            preview->enablePreview();
        else
            preview->disablePreview();
        plugins->destroyPreviousPlugin();

        checkSupportedBorderSizes();

	// inform buttonPositionWidget about the new factory...
	buttonPositionWidget->setDecorationFactory(plugins->factory() );

	currentName = styleToConfigLib( currentName );

	// Delete old plugin widget if it exists
	delete pluginObject;
	pluginObject = 0;

	// Use klibloader for library manipulation
	KLibLoader* loader = KLibLoader::self();

	// Free the old library if possible
	if (!oldLibraryName.isNull())
		loader->unloadLibrary( oldName );

	KLibrary* library = loader->library( currentName );
	if (library != NULL)
	{
                KLibrary::void_function_ptr alloc_ptr = library->resolveFunction("allocate_config");
		if (alloc_ptr != NULL)
		{
			allocatePlugin = (QObject* (*)(KConfigGroup& conf, QWidget* parent))alloc_ptr;
			pluginObject = (QObject*)(allocatePlugin( conf, pluginConfigWidget ));

			// connect required signals and slots together...
			connect( pluginObject, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) );
			connect( this, SIGNAL(pluginLoad(const KConfigGroup&)), pluginObject, SLOT(load(const KConfigGroup&)) );
			connect( this, SIGNAL(pluginSave(KConfigGroup &)), pluginObject, SLOT(save(KConfigGroup &)) );
			connect( this, SIGNAL(pluginDefaults()), pluginObject, SLOT(defaults()) );
                        pluginSettingsGrp->show();
			return;
		}
	}
        if ( cBorder->isHidden() )
            pluginSettingsGrp->hide();
        else if (pluginSettingsGrp->isHidden() )
            pluginSettingsGrp->show();
}


// Reads the kwin config settings, and sets all UI controls to those settings
// Updating the config plugin if required
void KWinDecorationModule::readConfig( const KConfigGroup & conf )
{
	// General tab
	// ============
	cbShowToolTips->setChecked( conf.readEntry("ShowToolTips", true));
//	cbUseMiniWindows->setChecked( conf.readEntry("MiniWindowBorders", false));

	// Find the corresponding decoration name to that of
	// the current plugin library name

	oldLibraryName = currentLibraryName;
	currentLibraryName = conf.readEntry("PluginLib",
					((QPixmap::defaultDepth() > 8) ? "kwin3_ozone" : "kwin3_plastik"));
	QString decoName = decorationName( currentLibraryName );

	// If we are using the "default" kde client, use the "default" entry.
//	if (decoName.isEmpty())
//		decoName = i18n("KDE 2");

    int numDecos = decorationList->count();
	for (int i = 0; i < numDecos; ++i)
    {
		 if (decorationList->itemText(i) == decoName)
		 {
		 		 decorationList->setCurrentIndex(i);
		 		 break;
		 }
	}

	// Buttons tab
	// ============
	bool customPositions = conf.readEntry("CustomButtonPositions", false);
	cbUseCustomButtonPositions->setChecked( customPositions );
	buttonPositionWidget->setEnabled( customPositions );
	// Menu and onAllDesktops buttons are default on LHS
	buttonPositionWidget->setButtonsLeft( conf.readEntry("ButtonsOnLeft", KDecorationOptions::defaultTitleButtonsLeft()) );
	// Help, Minimize, Maximize and Close are default on RHS
	buttonPositionWidget->setButtonsRight( conf.readEntry("ButtonsOnRight", KDecorationOptions::defaultTitleButtonsRight()) );

        int bsize = conf.readEntry( "BorderSize", (int)BorderNormal );
        if( bsize >= BorderTiny && bsize < BordersCount )
            border_size = static_cast< BorderSize >( bsize );
        else
            border_size = BorderNormal;
        checkSupportedBorderSizes();

	emit KCModule::changed(false);
}


// Writes the selected user configuration to the kwin config file
void KWinDecorationModule::writeConfig( KConfigGroup & conf )
{
	QString name = decorationList->currentText();
	QString libName = decorationLibName( name );

	KConfig _kwinConfig( "kwinrc" );
	KConfigGroup kwinConfig(&_kwinConfig, "Style");

	// General settings
	conf.writeEntry("PluginLib", libName);
	conf.writeEntry("CustomButtonPositions", cbUseCustomButtonPositions->isChecked());
	conf.writeEntry("ShowToolTips", cbShowToolTips->isChecked());
//	conf.writeEntry("MiniWindowBorders", cbUseMiniWindows->isChecked());

	// Button settings
	conf.writeEntry("ButtonsOnLeft", buttonPositionWidget->buttonsLeft() );
	conf.writeEntry("ButtonsOnRight", buttonPositionWidget->buttonsRight() );
	conf.writeEntry("BorderSize", static_cast<int>( border_size ) );

	oldLibraryName = currentLibraryName;
	currentLibraryName = libName;

	// We saved, so tell kcmodule that there have been  no new user changes made.
	emit KCModule::changed(false);
}


// Virutal functions required by KCModule
void KWinDecorationModule::load()
{
	KConfig _kwinConfig( "kwinrc" );
	KConfigGroup kwinConfig(&_kwinConfig, "Style");

	// Reset by re-reading the config
	readConfig( kwinConfig );
        resetPlugin( kwinConfig );
}


void KWinDecorationModule::save()
{
	KConfig _kwinConfig( "kwinrc" );
	KConfigGroup kwinConfig(&_kwinConfig, "Style");

	writeConfig( kwinConfig );
	emit pluginSave( kwinConfig );

	kwinConfig.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

}


void KWinDecorationModule::defaults()
{
	// Set the KDE defaults
	cbUseCustomButtonPositions->setChecked( false );
	buttonPositionWidget->setEnabled( false );
	cbShowToolTips->setChecked( true );
//	cbUseMiniWindows->setChecked( false);
// Don't set default for now
//	decorationList->setSelected(
//		decorationList->findItem( i18n("KDE 2") ), true );  // KDE classic client
	decorationList->setCurrentItem( i18n("Ozone"), true );  // KDE classic client
	slotChangeDecoration(i18n("Ozone"));

	buttonPositionWidget->setButtonsLeft(KDecorationOptions::defaultTitleButtonsLeft());
	buttonPositionWidget->setButtonsRight(KDecorationOptions::defaultTitleButtonsRight());

	border_size = BorderNormal;
	checkSupportedBorderSizes();

	// Set plugin defaults
	emit pluginDefaults();
}

void KWinDecorationModule::checkSupportedBorderSizes()
{
        QList< BorderSize > sizes;
        if( plugins->factory() != NULL )
            sizes = plugins->factory()->borderSizes();
	if( sizes.count() < 2 ) {
		lBorder->hide();
		cBorder->hide();
	} else {
		cBorder->clear();
		for (QList<BorderSize>::const_iterator it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
			BorderSize size = *it;
			cBorder->addItem(i18n(border_names[size]), borderSizeToIndex(size,sizes) );
		}
		int pos = borderSizeToIndex( border_size, sizes );
		lBorder->show();
		cBorder->show();
		cBorder->setCurrentIndex(pos);
		slotBorderChanged( pos );
	}
}

QString KWinDecorationModule::styleToConfigLib( QString& styleLib )
{
        if( styleLib.startsWith( "kwin3_" ))
            return "kwin_" + styleLib.mid( 6 ) + "_config";
        else
            return styleLib + "_config";
}

QString KWinDecorationModule::quickHelp() const
{
	return i18n( "<h1>Window Manager Decoration</h1>"
		"<p>This module allows you to choose the window border decorations, "
		"as well as titlebar button positions and custom decoration options.</p>"
		"To choose a theme for your window decoration click on its name and apply your choice by clicking the \"Apply\" button below."
		" If you do not want to apply your choice you can click the \"Reset\" button to discard your changes."
		"<p>You can configure each theme in the \"Configure [...]\" tab. There are different options specific for each theme.</p>"
		"<p>In \"General Options (if available)\" you can activate the \"Buttons\" tab by checking the \"Use custom titlebar button positions\" box."
		" In the \"Buttons\" tab you can change the positions of the buttons to your liking.</p>" );
}

#include "kwindecoration.moc"
// vim: ts=4
// kate: space-indent off; tab-width 4;

