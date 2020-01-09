/*
 * KCMStyle
 * Copyright (C) 2002 Karol Szwed <gallium@kde.org>
 * Copyright (C) 2002 Daniel Molkentin <molkentin@kde.org>
 * Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
 *
 * Portions Copyright (C) 2000 TrollTech AS.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kcmstyle.h"

#ifdef Q_WS_X11
#include <config-X11.h>
#endif

#include "styleconfdialog.h"
#include "ui_stylepreview.h"

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <kstyle.h>
#include <kstandarddirs.h>
#include <KDebug>

#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtGui/QLabel>
#include <QtGui/QPixmapCache>
#include <QtGui/QStyleFactory>
#include <QtGui/QFormLayout>
#include <QtDBus/QtDBus>

#ifdef Q_WS_X11
#include <QX11Info>
#endif

#include "../krdb/krdb.h"

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

// X11 namespace cleanup
#undef Below
#undef KeyPress
#undef KeyRelease


/**** DLL Interface for kcontrol ****/

#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <KLibLoader>

K_PLUGIN_FACTORY(KCMStyleFactory, registerPlugin<KCMStyle>();)
K_EXPORT_PLUGIN(KCMStyleFactory("kcmstyle"))


extern "C"
{
    KDE_EXPORT void kcminit_style()
    {
        uint flags = KRdbExportQtSettings | KRdbExportQtColors | KRdbExportXftSettings;
        KConfig _config( "kcmdisplayrc", KConfig::NoGlobals  );
        KConfigGroup config(&_config, "X11");

        // This key is written by the "colors" module.
        bool exportKDEColors = config.readEntry("exportKDEColors", true);
        if (exportKDEColors)
            flags |= KRdbExportColors;
        runRdb( flags );

        // Write some Qt root property.
#ifdef Q_WS_X11
#ifndef __osf__      // this crashes under Tru64 randomly -- will fix later
        QByteArray properties;
        QDataStream d(&properties, QIODevice::WriteOnly);
        d.setVersion( 3 );      // Qt2 apps need this.
        d << kapp->palette() << KGlobalSettings::generalFont();
        Atom a = XInternAtom(QX11Info::display(), "_QT_DESKTOP_PROPERTIES", false);

        // do it for all root windows - multihead support
        int screen_count = ScreenCount(QX11Info::display());
        for (int i = 0; i < screen_count; i++)
            XChangeProperty(QX11Info::display(), RootWindow(QX11Info::display(), i),
                            a, a, 8, PropModeReplace,
                            (unsigned char*) properties.data(), properties.size());
#endif
#endif
    }
}

class StylePreview : public QWidget, public Ui::StylePreview
{
public:
    StylePreview(QWidget *parent = 0)
    : QWidget(parent)
    {
        setupUi(this);

        // Ensure that the user can't toy with the child widgets.
        // Method borrowed from Qt's qtconfig.
        QList<QWidget*> widgets = findChildren<QWidget*>();
        foreach (QWidget* widget, widgets)
        {
            widget->installEventFilter(this);
            widget->setFocusPolicy(Qt::NoFocus);
        }
    }

    bool eventFilter( QObject* /* obj */, QEvent* ev )
    {
        switch( ev->type() )
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseMove:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Enter:
            case QEvent::Leave:
            case QEvent::Wheel:
            case QEvent::ContextMenu:
                  return true; // ignore
            default:
                break;
        }
        return false;
    }
};


KCMStyle::KCMStyle( QWidget* parent, const QVariantList& )
	: KCModule( KCMStyleFactory::componentData(), parent ), appliedStyle(NULL)
{
	setQuickHelp( i18n("<h1>Style</h1>"
			"This module allows you to modify the visual appearance "
			"of user interface elements, such as the widget style "
			"and effects."));

	m_bStyleDirty= false;
	m_bEffectsDirty = false;

	KGlobal::dirs()->addResourceType("themes", "data", "kstyle/themes");

	KAboutData *about =
		new KAboutData( I18N_NOOP("kcmstyle"), 0,
						ki18n("KDE Style Module"),
						0, KLocalizedString(), KAboutData::License_GPL,
						ki18n("(c) 2002 Karol Szwed, Daniel Molkentin"));

	about->addAuthor(ki18n("Karol Szwed"), KLocalizedString(), "gallium@kde.org");
	about->addAuthor(ki18n("Daniel Molkentin"), KLocalizedString(), "molkentin@kde.org");
	about->addAuthor(ki18n("Ralf Nolden"), KLocalizedString(), "nolden@kde.org");
	setAboutData( about );

	// Setup pages and mainLayout
	mainLayout = new QVBoxLayout( this );
	mainLayout->setMargin(0);

	tabWidget  = new QTabWidget( this );
	mainLayout->addWidget( tabWidget );

	page1 = new QWidget;
	page1Layout = new QVBoxLayout( page1 );

	// Add Page1 (Style)
	// -----------------
	//gbWidgetStyle = new QGroupBox( i18n("Widget Style"), page1 );
	QWidget* gbWidgetStyle = new QWidget( page1 );
	QVBoxLayout *widgetLayout = new QVBoxLayout(gbWidgetStyle);

	gbWidgetStyleLayout = new QVBoxLayout;
        widgetLayout->addLayout( gbWidgetStyleLayout );
	gbWidgetStyleLayout->setAlignment( Qt::AlignTop );
	hbLayout = new QHBoxLayout( );
	hbLayout->setObjectName( "hbLayout" );

	QLabel* label=new QLabel(i18n("Widget style:"),this);
	hbLayout->addWidget( label );

	cbStyle = new KComboBox( gbWidgetStyle );
        cbStyle->setObjectName( "cbStyle" );
	cbStyle->setEditable( false );
	hbLayout->addWidget( cbStyle );
	hbLayout->setStretchFactor( cbStyle, 1 );
	label->setBuddy(cbStyle);

	pbConfigStyle = new QPushButton( i18n("Con&figure..."), gbWidgetStyle );
	pbConfigStyle->setEnabled( false );
	hbLayout->addWidget( pbConfigStyle );

	gbWidgetStyleLayout->addLayout( hbLayout );

	lblStyleDesc = new QLabel( gbWidgetStyle );
	gbWidgetStyleLayout->addWidget( lblStyleDesc );

	QGroupBox *gbPreview = new QGroupBox( i18n( "Preview" ), page1 );
	QVBoxLayout *previewLayout = new QVBoxLayout(gbPreview);
	previewLayout->setMargin( 0 );
	stylePreview = new StylePreview( gbPreview );
	gbPreview->layout()->addWidget( stylePreview );

	page1Layout->addWidget( gbWidgetStyle );
	page1Layout->addWidget( gbPreview );
	page1Layout->addStretch();

	connect( cbStyle, SIGNAL(activated(int)), this, SLOT(styleChanged()) );
	connect( cbStyle, SIGNAL(activated(int)), this, SLOT(updateConfigButton()));
	connect( pbConfigStyle, SIGNAL(clicked()), this, SLOT(styleSpecificConfig()));

	// Add Page2 (Effects)
	// -------------------
	page2 = new QWidget;
	QFormLayout* page2Layout = new QFormLayout( page2 );

	comboGraphicEffectsLevel = new KComboBox( page2 );
	comboGraphicEffectsLevel->setVisible(false);
	comboGraphicEffectsLevel->setObjectName( "cbGraphicEffectsLevel" );
	comboGraphicEffectsLevel->setEditable( false );
	comboGraphicEffectsLevel->addItem(i18n("Low display resolution and Low CPU"), KGlobalSettings::NoEffects);
	comboGraphicEffectsLevel->addItem(i18n("High display resolution and Low CPU"), KGlobalSettings::GradientEffects);
	comboGraphicEffectsLevel->addItem(i18n("Low display resolution and High CPU"), KGlobalSettings::SimpleAnimationEffects);
	comboGraphicEffectsLevel->addItem(i18n("High display resolution and High CPU"), (int) (KGlobalSettings::SimpleAnimationEffects | KGlobalSettings::GradientEffects));
	comboGraphicEffectsLevel->addItem(i18n("Low display resolution and Very High CPU"), KGlobalSettings::ComplexAnimationEffects);
	comboGraphicEffectsLevel->addItem(i18n("High display resolution and Very High CPU"), (int) (KGlobalSettings::ComplexAnimationEffects | KGlobalSettings::GradientEffects));
	//page2Layout->addRow(i18nc("@label:listbox","Graphical User Interface:"), comboGraphicEffectsLevel);

	comboToolbarIcons = new QComboBox( page2 );
	comboToolbarIcons->setEditable( false );
	comboToolbarIcons->addItem( i18n("Icons Only") );
	comboToolbarIcons->addItem( i18n("Text Only") );
	comboToolbarIcons->addItem( i18n("Text Alongside Icons") );
	comboToolbarIcons->addItem( i18n("Text Under Icons") );
	comboToolbarIcons->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );
	page2Layout->addRow(i18nc("@label:listbox","Text pos&ition of toolbar elements:"), comboToolbarIcons);

	cbIconsOnButtons = new QCheckBox( i18nc("@option:check","Sho&w icons on buttons"), page2 );
 	page2Layout->addRow(cbIconsOnButtons);

	connect(cbStyle, SIGNAL(activated(int)), this, SLOT(setStyleDirty()));
	connect( cbIconsOnButtons,     SIGNAL(toggled(bool)),   this, SLOT(setEffectsDirty()));
	connect( comboGraphicEffectsLevel, SIGNAL(activated(int)),   this, SLOT(setEffectsDirty()));
	connect( comboToolbarIcons,    SIGNAL(activated(int)), this, SLOT(setEffectsDirty()));

	addWhatsThis();

	// Insert the pages into the tabWidget
	tabWidget->addTab( page1, i18nc("@title:tab","&Style"));
	tabWidget->addTab( page2, i18nc("@title:tab","&Fine Tuning"));

}


KCMStyle::~KCMStyle()
{
	qDeleteAll(styleEntries);
	delete appliedStyle;
}

void KCMStyle::updateConfigButton()
{
	if (!styleEntries[currentStyle()] || styleEntries[currentStyle()]->configPage.isEmpty()) {
		pbConfigStyle->setEnabled(false);
		return;
	}

	// We don't check whether it's loadable here -
	// lets us report an error and not waste time
	// loading things if the user doesn't click the button
	pbConfigStyle->setEnabled( true );
}

void KCMStyle::styleSpecificConfig()
{
	QString libname = styleEntries[currentStyle()]->configPage;

	KLibrary library(libname, KCMStyleFactory::componentData());
	if (!library.load()) {
		KMessageBox::detailedError(this,
			i18n("There was an error loading the configuration dialog for this style."),
			library.errorString(),
			i18n("Unable to Load Dialog"));
		return;
	}

    KLibrary::void_function_ptr allocPtr = library.resolveFunction("allocate_kstyle_config");

	if (!allocPtr)
	{
		KMessageBox::detailedError(this,
			i18n("There was an error loading the configuration dialog for this style."),
			library.errorString(),
			i18n("Unable to Load Dialog"));
		return;
	}

	//Create the container dialog
	StyleConfigDialog* dial = new StyleConfigDialog(this, styleEntries[currentStyle()]->name);
	dial->showButtonSeparator(true);

	typedef QWidget*(* factoryRoutine)( QWidget* parent );

	//Get the factory, and make the widget.
	factoryRoutine factory      = (factoryRoutine)(allocPtr); //Grmbl. So here I am on my
	//"never use C casts" moralizing streak, and I find that one can't go void* -> function ptr
	//even with a reinterpret_cast.

	QWidget*       pluginConfig = factory( dial );

	//Insert it in...
	dial->setMainWidget( pluginConfig );

	//..and connect it to the wrapper
	connect(pluginConfig, SIGNAL(changed(bool)), dial, SLOT(setDirty(bool)));
	connect(dial, SIGNAL(defaults()), pluginConfig, SLOT(defaults()));
	connect(dial, SIGNAL(save()), pluginConfig, SLOT(save()));

	if (dial->exec() == QDialog::Accepted  && dial->isDirty() ) {
		// Force re-rendering of the preview, to apply settings
		switchStyle(currentStyle(), true);

		//For now, ask all KDE apps to recreate their styles to apply the setitngs
		KGlobalSettings::self()->emitChange(KGlobalSettings::StyleChanged);

		// We call setStyleDirty here to make sure we force style re-creation
		setStyleDirty();
	}

	delete dial;
}

void KCMStyle::changeEvent( QEvent *event )
{
	KCModule::changeEvent( event );
	if ( event->type() == QEvent::PaletteChange ) {
		// Force re-rendering of the preview, to apply new palette
		switchStyle(currentStyle(), true);
	}
}

void KCMStyle::load()
{
	KConfig config( "kdeglobals", KConfig::FullConfig );

	loadStyle( config );
	loadEffects( config );

	m_bStyleDirty= false;
	m_bEffectsDirty = false;
	//Enable/disable the button for the initial style
	updateConfigButton();

	emit changed( false );
}


void KCMStyle::save()
{
	// Don't do anything if we don't need to.
	if ( !( m_bStyleDirty | m_bEffectsDirty ) )
		return;

	// Save effects.
        KConfig      _config("kdeglobals", KConfig::NoGlobals);                
        KConfigGroup config(&_config, "KDE"); 
	// Effects page
	config.writeEntry( "ShowIconsOnPushButtons", cbIconsOnButtons->isChecked());
	KConfigGroup g( &_config, "KDE-Global GUI Settings" );
	g.writeEntry( "GraphicEffectsLevel", comboGraphicEffectsLevel->itemData(comboGraphicEffectsLevel->currentIndex()));

	KConfigGroup generalGroup(&_config, "General");
	generalGroup.writeEntry("widgetStyle", currentStyle());

	KConfigGroup toolbarStyleGroup(&_config, "Toolbar style");
	QString tbIcon;
	switch( comboToolbarIcons->currentIndex() )
	{
		case 0: tbIcon = "IconOnly"; break;
		case 1: tbIcon = "TextOnly"; break;
		case 2: tbIcon = "TextBesideIcon"; break;
		default:
		case 3: tbIcon = "TextUnderIcon"; break;
	}
	toolbarStyleGroup.writeEntry("ToolButtonStyle", tbIcon);
	_config.sync();

	// Export the changes we made to qtrc, and update all qt-only
	// applications on the fly, ensuring that we still follow the user's
	// export fonts/colors settings.
	if (m_bStyleDirty | m_bEffectsDirty)	// Export only if necessary
	{
		uint flags = KRdbExportQtSettings;
		KConfig _kconfig( "kcmdisplayrc", KConfig::NoGlobals  );
		KConfigGroup kconfig(&_kconfig, "X11");
		bool exportKDEColors = kconfig.readEntry("exportKDEColors", true);
		if (exportKDEColors)
			flags |= KRdbExportColors;
		runRdb( flags );
	}

	// Now allow KDE apps to reconfigure themselves.
	if ( m_bStyleDirty )
		KGlobalSettings::self()->emitChange(KGlobalSettings::StyleChanged);

	if ( m_bEffectsDirty ) {
		KGlobalSettings::self()->emitChange(KGlobalSettings::SettingsChanged);
		// ##### FIXME - Doesn't apply all settings correctly due to bugs in
		// KApplication/KToolbar
		KGlobalSettings::self()->emitChange(KGlobalSettings::ToolbarStyleChanged);

#ifdef Q_WS_X11
		// Send signal to all kwin instances
		QDBusMessage message =
		QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
		QDBusConnection::sessionBus().send(message);
#endif
	}

	// Clean up
	m_bStyleDirty    = false;
	m_bEffectsDirty  = false;
	emit changed( false );
}


bool KCMStyle::findStyle( const QString& str, int& combobox_item )
{
	StyleEntry* se   = styleEntries[str.toLower()];

	QString     name = se ? se->name : str;

	combobox_item = 0;

	//look up name
	for( int i = 0; i < cbStyle->count(); i++ )
	{
		if ( cbStyle->itemText(i) == name )
		{
			combobox_item = i;
			return true;
		}
	}

	return false;
}


void KCMStyle::defaults()
{
	// Select default style
	int item = 0;
	bool found;

	found = findStyle( KStyle::defaultStyle(), item );
	if (!found)
		found = findStyle( "plastique", item );
	if (!found)
		found = findStyle( "windows", item );
	if (!found)
		found = findStyle( "platinum", item );
	if (!found)
		found = findStyle( "motif", item );

	cbStyle->setCurrentIndex( item );

	m_bStyleDirty = true;
	switchStyle( currentStyle() );	// make resets visible

	// Effects
	comboToolbarIcons->setCurrentIndex(0);
	cbIconsOnButtons->setChecked(true);
	comboGraphicEffectsLevel->setCurrentIndex(comboGraphicEffectsLevel->findData(((int) KGlobalSettings::graphicEffectsLevelDefault())));
	emit changed(true);
}

void KCMStyle::setEffectsDirty()
{
	m_bEffectsDirty = true;
	emit changed(true);
}

void KCMStyle::setStyleDirty()
{
	m_bStyleDirty = true;
	emit changed(true);
}

// ----------------------------------------------------------------
// All the Style Switching / Preview stuff
// ----------------------------------------------------------------

void KCMStyle::loadStyle( KConfig& config )
{
	cbStyle->clear();
	// Create a dictionary of WidgetStyle to Name and Desc. mappings,
	// as well as the config page info
	qDeleteAll(styleEntries);
	styleEntries.clear();

	QString strWidgetStyle;
	QStringList list = KGlobal::dirs()->findAllResources("themes", "*.themerc",
														 KStandardDirs::Recursive |
														 KStandardDirs::NoDuplicates);
	for (QStringList::iterator it = list.begin(); it != list.end(); ++it)
	{
		KConfig config(  *it, KConfig::SimpleConfig);
		if ( !(config.hasGroup("KDE") && config.hasGroup("Misc")) )
			continue;

		KConfigGroup configGroup = config.group("KDE");

		strWidgetStyle = configGroup.readEntry("WidgetStyle");
		if (strWidgetStyle.isNull())
			continue;

		// We have a widgetstyle, so lets read the i18n entries for it...
		StyleEntry* entry = new StyleEntry;
		configGroup = config.group("Misc");
		entry->name = configGroup.readEntry("Name");
		entry->desc = configGroup.readEntry("Comment", i18n("No description available."));
		entry->configPage = configGroup.readEntry("ConfigPage", QString());

		// Check if this style should be shown
		configGroup = config.group("Desktop Entry");
		entry->hidden = configGroup.readEntry("Hidden", false);

		// Insert the entry into our dictionary.
		styleEntries.insert(strWidgetStyle.toLower(), entry);
	}

	// Obtain all style names
	QStringList allStyles = QStyleFactory::keys();

	// Get translated names, remove all hidden style entries.
	QStringList styles;
	StyleEntry* entry;
	for (QStringList::iterator it = allStyles.begin(); it != allStyles.end(); ++it)
	{
		QString id = (*it).toLower();
		// Find the entry.
		if ( (entry = styleEntries[id]) != 0 )
		{
			// Do not add hidden entries
			if (entry->hidden)
				continue;

			styles += entry->name;

			nameToStyleKey[entry->name] = id;
		}
		else
		{
			styles += (*it); //Fall back to the key (but in original case)
			nameToStyleKey[*it] = id;
		}
	}

	// Sort the style list, and add it to the combobox
	styles.sort();
	cbStyle->addItems( styles );

	// Find out which style is currently being used
	KConfigGroup configGroup = config.group( "General" );
	QString defaultStyle = KStyle::defaultStyle();
	QString cfgStyle = configGroup.readEntry( "widgetStyle", defaultStyle );

	// Select the current style
	// Do not use cbStyle->listBox() as this may be NULL for some styles when
	// they use QPopupMenus for the drop-down list!

	// ##### Since Trolltech likes to seemingly copy & paste code,
	// QStringList::findItem() doesn't have a Qt::StringComparisonMode field.
	// We roll our own (yuck)
	cfgStyle = cfgStyle.toLower();
	int item = 0;
	for( int i = 0; i < cbStyle->count(); i++ )
	{
		QString id = nameToStyleKey[cbStyle->itemText(i)];
		item = i;
		if ( id == cfgStyle )	// ExactMatch
			break;
		else if ( id.contains( cfgStyle ) )
			break;
		else if ( id.contains( QApplication::style()->metaObject()->className() ) )
			break;
		item = 0;
	}
	cbStyle->setCurrentIndex( item );
	m_bStyleDirty = false;

	switchStyle( currentStyle() );	// make resets visible
}

QString KCMStyle::currentStyle()
{
	return nameToStyleKey[cbStyle->currentText()];
}


void KCMStyle::styleChanged()
{
	switchStyle( currentStyle() );
}


void KCMStyle::switchStyle(const QString& styleName, bool force)
{
	// Don't flicker the preview if the same style is chosen in the cb
	if (!force && appliedStyle && appliedStyle->objectName() == styleName)
		return;

	// Create an instance of the new style...
	QStyle* style = QStyleFactory::create(styleName);
	if (!style)
		return;

	// Prevent Qt from wrongly caching radio button images
	QPixmapCache::clear();

	setStyleRecursive( stylePreview, style );

	// this flickers, but reliably draws the widgets correctly.
	stylePreview->resize( stylePreview->sizeHint() );

	delete appliedStyle;
	appliedStyle = style;

	// Set the correct style description
	StyleEntry* entry = styleEntries[ styleName ];
	QString desc;
	desc = i18n("Description: %1", entry ? entry->desc : i18n("No description available.") );
	lblStyleDesc->setText( desc );
}

void KCMStyle::setStyleRecursive(QWidget* w, QStyle* s)
{
	// Don't let broken styles kill the palette
	// for other styles being previewed. (e.g SGI style)
	w->setPalette(QPalette());

	QPalette newPalette(KGlobalSettings::createApplicationPalette());
	s->polish( newPalette );
	w->setPalette(newPalette);

	// Apply the new style.
	w->setStyle(s);

	// Recursively update all children.
	const QObjectList children = w->children();

	// Apply the style to each child widget.
    foreach (QObject* child, children)
	{
		if (child->isWidgetType())
			setStyleRecursive((QWidget *) child, s);
	}
}

// ----------------------------------------------------------------
// All the Effects stuff
// ----------------------------------------------------------------

void KCMStyle::loadEffects( KConfig& config )
{
	// KDE's Part via KConfig
	KConfigGroup configGroup = config.group("Toolbar style");

	QString tbIcon = configGroup.readEntry("ToolButtonStyle", "TextUnderIcon");
	if (tbIcon == "TextOnly")
		comboToolbarIcons->setCurrentIndex(1);
	else if (tbIcon == "TextBesideIcon")
		comboToolbarIcons->setCurrentIndex(2);
	else if (tbIcon == "TextUnderIcon")
		comboToolbarIcons->setCurrentIndex(3);
	else
		comboToolbarIcons->setCurrentIndex(0);

	configGroup = config.group("KDE");
	cbIconsOnButtons->setChecked(configGroup.readEntry("ShowIconsOnPushButtons", true));

	KConfigGroup graphicConfigGroup = config.group("KDE-Global GUI Settings");
	comboGraphicEffectsLevel->setCurrentIndex(comboGraphicEffectsLevel->findData(graphicConfigGroup.readEntry("GraphicEffectsLevel", ((int) KGlobalSettings::graphicEffectsLevel()))));

	m_bEffectsDirty = false;
}

void KCMStyle::addWhatsThis()
{
	// Page1
	cbStyle->setWhatsThis( i18n("Here you can choose from a list of"
							" predefined widget styles (e.g. the way buttons are drawn) which"
							" may or may not be combined with a theme (additional information"
							" like a marble texture or a gradient).") );
	stylePreview->setWhatsThis( i18n("This area shows a preview of the currently selected style "
							"without having to apply it to the whole desktop.") );
	// Page2
	page2->setWhatsThis( i18n("This page allows you to choose details about the widget style options") );
	comboToolbarIcons->setWhatsThis( i18n( "<p><b>Icons only:</b> Shows only icons on toolbar buttons. "
							"Best option for low resolutions.</p>"
							"<p><b>Text only: </b>Shows only text on toolbar buttons.</p>"
							"<p><b>Text alongside icons: </b> Shows icons and text on toolbar buttons. "
							"Text is aligned alongside the icon.</p>"
							"<b>Text under icons: </b> Shows icons and text on toolbar buttons. "
							"Text is aligned below the icon.") );
	cbIconsOnButtons->setWhatsThis( i18n( "If you enable this option, KDE Applications will "
							"show small icons alongside some important buttons.") );
	comboGraphicEffectsLevel->setWhatsThis( i18n( "If you enable this option, KDE Applications will "
							"run internal animations.") );
}

#include "kcmstyle.moc"

// vim: set noet ts=4:
