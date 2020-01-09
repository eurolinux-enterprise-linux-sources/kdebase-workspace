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

#ifndef KWINDECORATION_H
#define KWINDECORATION_H

#include <kcmodule.h>
#include "buttons.h"
#include <kconfig.h>
#include <klibloader.h>

#include <kdecoration.h>

//Added by qt3to4:
#include <QLabel>
#include <kvbox.h>

class KComboBox;
class QCheckBox;
class QLabel;
class QTabWidget;
class KVBox;

class KDecorationPlugins;
class KDecorationPreview;

// Stores themeName and its corresponding library Name
struct DecorationInfo
{
	QString name;
	QString libraryName;
};


class KWinDecorationModule : public KCModule, public KDecorationDefines
{
	Q_OBJECT

	public:
		KWinDecorationModule(QWidget* parent, const QVariantList &);
		~KWinDecorationModule();

		virtual void load();
		virtual void save();
		virtual void defaults();

		QString quickHelp() const;

	signals:
		void pluginLoad( const KConfigGroup& conf );
		void pluginSave( KConfigGroup &conf );
		void pluginDefaults();

	protected slots:
		// Allows us to turn "save" on
		void slotSelectionChanged();
		void slotChangeDecoration( const QString &  );
		void slotBorderChanged( int );
		void slotButtonsChanged();

	private:
		void readConfig( const KConfigGroup& conf );
		void writeConfig( KConfigGroup &conf );
		void findDecorations();
		void createDecorationList();
		void updateSelection();
		QString decorationLibName( const QString& name );
		QString decorationName ( QString& libName );
		static QString styleToConfigLib( QString& styleLib );
		void resetPlugin( KConfigGroup& conf, const QString& currentDecoName = QString() );
		void checkSupportedBorderSizes();
		static int borderSizeToIndex( BorderSize size, QList< BorderSize > sizes );
		static BorderSize indexToBorderSize( int index, QList< BorderSize > sizes );

		QTabWidget* tabWidget;

		// Page 1
		KComboBox* decorationList;
		QList<DecorationInfo> decorations;

		KDecorationPreview* preview;
		KDecorationPlugins* plugins;
		KSharedConfigPtr kwinConfig;

		QCheckBox* cbUseCustomButtonPositions;
	//	QCheckBox* cbUseMiniWindows;
		QCheckBox* cbShowToolTips;
		QLabel*    lBorder;
		QComboBox* cBorder;
		BorderSize border_size;

		QObject* pluginObject;
		QWidget* pluginConfigWidget;
		QString  currentLibraryName;
		QString  oldLibraryName;
		QObject* (*allocatePlugin)( KConfigGroup& conf, QWidget* parent );

		// Page 2
		ButtonPositionWidget *buttonPositionWidget;
		KVBox*	 buttonPage;
                QGroupBox *pluginSettingsGrp;
};


#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
