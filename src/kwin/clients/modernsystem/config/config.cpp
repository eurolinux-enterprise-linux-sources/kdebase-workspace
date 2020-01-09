/********************************************************************
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

// Melchior FRANZ  <mfranz@kde.org>	-- 2001-04-22

#include "config.h"

#include <kapplication.h>
#include <kconfig.h>
#include <kdialog.h>
#include <klocale.h>
#include <kglobal.h>
#include <QLayout>

//Added by qt3to4:
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <kvbox.h>


extern "C"
{
	KDE_EXPORT QObject* allocate_config(KConfig* conf, QWidget* parent)
	{
		return(new ModernSysConfig(conf, parent));
	}
}


// 'conf'	is a pointer to the kwindecoration modules open kwin config,
//		and is by default set to the "Style" group.
//
// 'parent'	is the parent of the QObject, which is a VBox inside the
//		Configure tab in kwindecoration

ModernSysConfig::ModernSysConfig(KConfig* conf, QWidget* parent) : QObject(parent)
{
	clientrc = new KConfig("kwinmodernsysrc");
	KGlobal::locale()->insertCatalog("kwin_clients");
	mainw = new QWidget(parent);
	vbox = new QVBoxLayout(mainw);
	vbox->setSpacing(6);
	vbox->setMargin(0);

	handleBox = new QWidget(mainw);
        QGridLayout* layout = new QGridLayout(handleBox );
        layout->setSpacing( KDialog::spacingHint() );

	cbShowHandle = new QCheckBox(i18n("&Show window resize handle"), handleBox);
	cbShowHandle->setWhatsThis(
			i18n("When selected, all windows are drawn with a resize "
			"handle at the lower right corner. This makes window resizing "
			"easier, especially for trackballs and other mouse replacements "
			"on laptops."));
        layout->addWidget(cbShowHandle, 0, 0, 1, 2 );
	connect(cbShowHandle, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()));

	sliderBox = new KVBox(handleBox);
	//handleSizeSlider = new QSlider(0, 4, 1, 0, Qt::Horizontal, sliderBox);
	handleSizeSlider = new QSlider(Qt::Horizontal, sliderBox);
	handleSizeSlider->setMinimum(0);
	handleSizeSlider->setMaximum(4);
	handleSizeSlider->setPageStep(1);
	handleSizeSlider->setWhatsThis(
			i18n("Here you can change the size of the resize handle."));
	handleSizeSlider->setTickInterval(1);
	handleSizeSlider->setTickPosition(QSlider::TicksBelow);
	connect(handleSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSelectionChanged()));

	hbox = new KHBox(sliderBox);
	hbox->setSpacing(6);

	bool rtl = kapp->layoutDirection() == Qt::RightToLeft;
	label1 = new QLabel(i18n("Small"), hbox);
	label1->setAlignment(rtl ? Qt::AlignRight : Qt::AlignLeft);
	label2 = new QLabel(i18n("Medium"), hbox);
	label2->setAlignment( Qt::AlignHCenter );
	label3 = new QLabel(i18n("Large"), hbox);
	label3->setAlignment(rtl ? Qt::AlignLeft : Qt::AlignRight);

	vbox->addWidget(handleBox);
	vbox->addStretch(1);

//        layout->setColumnMinimumWidth(0, 30);
        layout->addItem(new QSpacerItem(30, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
        layout->addWidget(sliderBox, 1, 1);

        KConfigGroup group(conf,"General");
	load(group);
	mainw->show();
}


ModernSysConfig::~ModernSysConfig()
{
	delete mainw;
	delete clientrc;
}


void ModernSysConfig::slotSelectionChanged()
{
	bool i = cbShowHandle->isChecked();
	if (i != hbox->isEnabled()) {
		hbox->setEnabled(i);
		handleSizeSlider->setEnabled(i);
	}
	emit changed();
}


void ModernSysConfig::load(const KConfigGroup& /*conf*/)
{
	KConfigGroup cg(clientrc, "General");
	bool i = cg.readEntry("ShowHandle", true);
	cbShowHandle->setChecked(i);
	hbox->setEnabled(i);
	handleSizeSlider->setEnabled(i);
	handleWidth = cg.readEntry("HandleWidth", 6);
	handleSize = cg.readEntry("HandleSize", 30);
	handleSizeSlider->setValue(qMin((handleWidth - 6) / 2, (uint)4));

}


void ModernSysConfig::save(KConfigGroup& /*conf*/)
{
	KConfigGroup cg(clientrc, "General");
	cg.writeEntry("ShowHandle", cbShowHandle->isChecked());
	cg.writeEntry("HandleWidth", 6 + 2 * handleSizeSlider->value());
	cg.writeEntry("HandleSize", 30 + 4 * handleSizeSlider->value());
	clientrc->sync();
}


void ModernSysConfig::defaults()
{
	cbShowHandle->setChecked(true);
	hbox->setEnabled(true);
	handleSizeSlider->setEnabled(true);
	handleSizeSlider->setValue(0);
}

#include "config.moc"
