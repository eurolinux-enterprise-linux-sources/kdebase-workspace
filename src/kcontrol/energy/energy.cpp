/* vi: ts=8 sts=4 sw=4
 *
 *
 *
 * This file is part of the KDE project, module kcontrol.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 *
 * You can Freely distribute this program under the GNU General Public
 * License. See the file "COPYING" for the exact licensing terms.
 *
 * Based on kcontrol1 energy.cpp, Copyright (c) 1999 Tom Vijlbrief
 */


/*
 * KDE Energy setup module.
 */

#include <config-workspace.h>

#if !defined(QT_CLEAN_NAMESPACE)
#define QT_CLEAN_NAMESPACE
#endif

#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <kcomponentdata.h>

//Added by qt3to4:
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <kconfig.h>
#include <kcursor.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <klocale.h>
#include <knuminput.h>
#include <krun.h>
#include <kstandarddirs.h>
#include <kurllabel.h>
#include <kgenericfactory.h>

#include <kscreensaver_interface.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "energy.h"
#include <QX11Info>


#ifdef HAVE_DPMS
#include <X11/Xmd.h>
extern "C" {
#include <X11/extensions/dpms.h>
Status DPMSInfo ( Display *, CARD16 *, BOOL * );
Bool DPMSCapable( Display * );
int __kde_do_not_unload = 1;

#ifndef HAVE_DPMSCAPABLE_PROTO
Bool DPMSCapable ( Display * );
#endif

#ifndef HAVE_DPMSINFO_PROTO
Status DPMSInfo ( Display *, CARD16 *, BOOL * );
#endif
}

#if defined(XIMStringConversionRetrival) || defined (__sun) || defined(__hpux)
extern "C" {
#endif
    Bool DPMSQueryExtension(Display *, int *, int *);
    Status DPMSEnable(Display *);
    Status DPMSDisable(Display *);
    Bool DPMSGetTimeouts(Display *, CARD16 *, CARD16 *, CARD16 *);
    Bool DPMSSetTimeouts(Display *, CARD16, CARD16, CARD16);
#if defined(XIMStringConversionRetrival) || defined (__sun) || defined(__hpux)
}
#endif
#endif

static const int DFLT_STANDBY   = 0;
static const int DFLT_SUSPEND   = 30;
static const int DFLT_OFF   = 60;


/**** DLL Interface ****/


K_PLUGIN_FACTORY(KEnergyFactory,
        registerPlugin<KEnergy>();
        )
K_EXPORT_PLUGIN(KEnergyFactory("kcmenergy"))


extern "C" {

    KDE_EXPORT void kcminit_energy() {
#ifdef HAVE_DPMS
        KConfig *_cfg = new KConfig( "kcmdisplayrc", KConfig::NoGlobals );
        KConfigGroup cfg(_cfg, "DisplayEnergy");

	Display *dpy = QX11Info::display();
	CARD16 pre_configured_status;
	BOOL pre_configured_enabled;
	CARD16 pre_configured_standby;
	CARD16 pre_configured_suspend;
	CARD16 pre_configured_off;
        bool enabled;
        CARD16 standby;
        CARD16 suspend;
        CARD16 off;
	int dummy;
	/* query the running X server if DPMS is supported */
	if (DPMSQueryExtension(dpy, &dummy, &dummy) && DPMSCapable(dpy)) {
	    DPMSGetTimeouts(dpy, &pre_configured_standby, &pre_configured_suspend, &pre_configured_off);
	    DPMSInfo(dpy, &pre_configured_status, &pre_configured_enabled);
	    /* let the user override the settings */
	    enabled = cfg.readEntry("displayEnergySaving", bool(pre_configured_enabled));
	    standby = cfg.readEntry("displayStandby", int(pre_configured_standby/60));
	    suspend = cfg.readEntry("displaySuspend", int(pre_configured_suspend/60));
	    off = cfg.readEntry("displayPowerOff", int(pre_configured_off/60));
	} else {
	/* provide our defauts */
	    enabled = true;
	    standby = DFLT_STANDBY;
	    suspend = DFLT_SUSPEND;
	    off = DFLT_OFF;
	}
        delete _cfg;
        KEnergy::applySettings(enabled, standby, suspend, off);
#endif
    }
}

/**** KEnergy ****/

KEnergy::KEnergy(QWidget *parent, const QVariantList &args)
    : KCModule(KEnergyFactory::componentData(), parent, args)
{
    m_bChanged = false;
    m_bEnabled = false;
    m_Standby = DFLT_STANDBY;
    m_Suspend = DFLT_SUSPEND;
    m_Off = DFLT_OFF;
    m_bDPMS = false;
    m_bMaintainSanity = true;

    setQuickHelp( i18n("<h1>Display Power Control</h1> <p>If your display supports"
      " power saving features, you can configure them using this module.</p>"
      " <p>There are three levels of power saving: standby, suspend, and off."
      " The greater the level of power saving, the longer it takes for the"
      " display to return to an active state.</p>"
      " <p>To wake up the display from a power saving mode, you can make a small"
      " movement with the mouse, or press a key that is not likely to cause"
      " any unintentional side-effects, for example, the \"Shift\" key.</p>"));

#ifdef HAVE_DPMS
    int dummy;
    m_bDPMS = DPMSQueryExtension(QX11Info::display(), &dummy, &dummy);
#endif

    QVBoxLayout *top = new QVBoxLayout(this);
    top->setMargin(0);
    top->setSpacing(KDialog::spacingHint());
    QHBoxLayout *hbox = new QHBoxLayout();
    top->addLayout(hbox);

    QLabel *lbl;
    if (m_bDPMS) {
    m_pCBEnable= new QCheckBox(i18n("&Enable display power management" ), this);
    connect(m_pCBEnable, SIGNAL(toggled(bool)), SLOT(slotChangeEnable(bool)));
    hbox->addWidget(m_pCBEnable);
        m_pCBEnable->setWhatsThis( i18n("Check this option to enable the"
           " power saving features of your display.") );
    } else {
        lbl = new QLabel(i18n("Your display does not support power saving."), this);
         hbox->addWidget(lbl);
    }

#if 0    // Re-enable when / if we have permission to use official logo
    KUrlLabel *logo = new KUrlLabel(this);
    logo->setUrl("http://www.energystar.gov");
    logo->setPixmap(QPixmap(KStandardDirs::locate("data", "kcontrol/pics/energybig.png")));
    logo->setTipText(i18n("Learn more about the Energy Star program"));
    logo->setUseTips(true);
    connect(logo, SIGNAL(leftClickedUrl(const QString&)), SLOT(openUrl(const QString &)));

    hbox->addStretch();
    hbox->addWidget(logo);
#endif

    // Sliders
    m_pStandbySlider = new KIntNumInput(m_Standby, this);
    m_pStandbySlider->setLabel(i18n("&Standby after:"));
    m_pStandbySlider->setRange(0, 120, 1);
    m_pStandbySlider->setSuffix(i18n(" min"));
    m_pStandbySlider->setSpecialValueText(i18nc("Disables display entering standby mode after x min", "Disabled"));
    connect(m_pStandbySlider, SIGNAL(valueChanged(int)), SLOT(slotChangeStandby(int)));
    top->addWidget(m_pStandbySlider);
    m_pStandbySlider->setWhatsThis( i18n("Choose the period of inactivity"
       " after which the display should enter \"standby\" mode. This is the"
       " first level of power saving.") );

    m_pSuspendSlider = new KIntNumInput(m_pStandbySlider, m_Suspend, this);
    m_pSuspendSlider->setLabel(i18n("S&uspend after:"));
    m_pSuspendSlider->setRange(0, 120, 1);
    m_pSuspendSlider->setSuffix(i18n(" min"));
    m_pSuspendSlider->setSpecialValueText(i18nc("Disables display entering suspend mode after x min", "Disabled"));
    connect(m_pSuspendSlider, SIGNAL(valueChanged(int)), SLOT(slotChangeSuspend(int)));
    top->addWidget(m_pSuspendSlider);
    m_pSuspendSlider->setWhatsThis( i18n("Choose the period of inactivity"
       " after which the display should enter \"suspend\" mode. This is the"
       " second level of power saving, but may not be different from the first"
       " level for some displays.") );

    m_pOffSlider = new KIntNumInput(m_pSuspendSlider, m_Off, this);
    m_pOffSlider->setLabel(i18n("&Power off after:"));
    m_pOffSlider->setRange(0, 120, 1);
    m_pOffSlider->setSuffix(i18n(" min"));
    m_pOffSlider->setSpecialValueText(i18nc("Disables display being turned off after x min", "Disabled"));
    connect(m_pOffSlider, SIGNAL(valueChanged(int)), SLOT(slotChangeOff(int)));
    top->addWidget(m_pOffSlider);
    m_pOffSlider->setWhatsThis( i18n("Choose the period of inactivity"
       " after which the display should be powered off. This is the"
       " greatest level of power saving that can be achieved while the"
       " display is still physically turned on.") );

    top->addStretch();

    if (m_bDPMS)
       setButtons( KCModule::Help | KCModule::Default | KCModule::Apply );
    else
       setButtons( KCModule::Help );

    m_pConfig = new KConfig("kcmdisplayrc", KConfig::NoGlobals);
}


KEnergy::~KEnergy()
{
    delete m_pConfig;
}



void KEnergy::load()
{
    readSettings();
    showSettings();

    emit changed(false);
}


void KEnergy::save()
{
    writeSettings();
    applySettings(m_bEnabled, m_Standby, m_Suspend, m_Off);

    emit changed(false);
}


void KEnergy::defaults()
{
    m_bEnabled = false;
    m_Standby = DFLT_STANDBY;
    m_Suspend = DFLT_SUSPEND;
    m_Off = DFLT_OFF;

    m_StandbyDesired = m_Standby;
    m_SuspendDesired = m_Suspend;
    m_OffDesired = m_Off;

    showSettings();
    emit changed(true);
}


void KEnergy::readSettings()
{
    KConfigGroup group = m_pConfig->group("DisplayEnergy");
    m_bEnabled = group.readEntry("displayEnergySaving", false);
    m_Standby = group.readEntry("displayStandby", DFLT_STANDBY);
    m_Suspend = group.readEntry("displaySuspend", DFLT_SUSPEND);
    m_Off = group.readEntry("displayPowerOff", DFLT_OFF);

    m_StandbyDesired = m_Standby;
    m_SuspendDesired = m_Suspend;
    m_OffDesired = m_Off;

    m_bChanged = false;
}


void KEnergy::writeSettings()
{
    if (!m_bChanged)
     return;

    KConfigGroup group = m_pConfig->group("DisplayEnergy");
    group.writeEntry( "displayEnergySaving", m_bEnabled);
    group.writeEntry("displayStandby", m_Standby);
    group.writeEntry("displaySuspend", m_Suspend);
    group.writeEntry("displayPowerOff", m_Off);

    group.sync();
    m_bChanged = false;
}


void KEnergy::showSettings()
{
    m_bMaintainSanity = false;

    if (m_bDPMS)
    m_pCBEnable->setChecked(m_bEnabled);

    m_pStandbySlider->setEnabled(m_bEnabled);
    m_pStandbySlider->setValue(m_Standby);
    m_pSuspendSlider->setEnabled(m_bEnabled);
    m_pSuspendSlider->setValue(m_Suspend);
    m_pOffSlider->setEnabled(m_bEnabled);
    m_pOffSlider->setValue(m_Off);

    m_bMaintainSanity = true;
}


extern "C" {
  int dropError(Display *, XErrorEvent *);
  typedef int (*XErrFunc) (Display *, XErrorEvent *);
}

int dropError(Display *, XErrorEvent *)
{
    return 0;
}

/* static */
void KEnergy::applySettings(bool enable, int standby, int suspend, int off)
{
#ifdef HAVE_DPMS
    XErrFunc defaultHandler;
    defaultHandler = XSetErrorHandler(dropError);

    Display *dpy = QX11Info::display();

    int dummy;
    bool hasDPMS = DPMSQueryExtension(dpy, &dummy, &dummy);
    if (hasDPMS) {
    if (enable) {
            DPMSEnable(dpy);
            DPMSSetTimeouts(dpy, 60*standby, 60*suspend, 60*off);
        } else
            DPMSDisable(dpy);
    } else
    qWarning("Server has no DPMS extension");

    XFlush(dpy);
    XSetErrorHandler(defaultHandler);

    // The screen saver depends on the DPMS settings
    org::kde::screensaver kscreensaver("org.freedesktop.ScreenSaver", "/ScreenSaver", QDBusConnection::sessionBus());
    kscreensaver.configure();
#else
    /* keep gcc silent */
    if (enable | standby | suspend | off)
    /* nothing */ ;
#endif
}


void KEnergy::slotChangeEnable(bool ena)
{
    m_bEnabled = ena;
    m_bChanged = true;

    m_pStandbySlider->setEnabled(ena);
    m_pSuspendSlider->setEnabled(ena);
    m_pOffSlider->setEnabled(ena);

    emit changed(true);
}


void KEnergy::slotChangeStandby(int value)
{
    m_Standby = value;

    if ( m_bMaintainSanity ) {
	m_bMaintainSanity = false;
	m_StandbyDesired = value;
        if ((m_Suspend > 0 && m_Standby > m_Suspend) ||
	    (m_SuspendDesired && m_Standby >= m_SuspendDesired) )
            m_pSuspendSlider->setValue(m_Standby);
        if ((m_Off > 0 && m_Standby > m_Off) ||
	    (m_OffDesired && m_Standby >= m_OffDesired) )
            m_pOffSlider->setValue(m_Standby);
	m_bMaintainSanity = true;
    }

    m_bChanged = true;
    emit changed(true);
}


void KEnergy::slotChangeSuspend(int value)
{
    m_Suspend = value;

    if ( m_bMaintainSanity ) {
	m_bMaintainSanity = false;
	m_SuspendDesired = value;
	if (m_Suspend == 0 && m_StandbyDesired > 0)
	    m_pStandbySlider->setValue( m_StandbyDesired );
        else if (m_Suspend < m_Standby || m_Suspend <= m_StandbyDesired )
            m_pStandbySlider->setValue(m_Suspend);
        if ((m_Off > 0 && m_Suspend > m_Off) ||
	    (m_OffDesired && m_Suspend >= m_OffDesired) )
            m_pOffSlider->setValue(m_Suspend);
	m_bMaintainSanity = true;
    }

    m_bChanged = true;
    emit changed(true);
}


void KEnergy::slotChangeOff(int value)
{
    m_Off = value;

    if ( m_bMaintainSanity ) {
	m_bMaintainSanity = false;
	m_OffDesired = value;
	if (m_Off == 0 && m_StandbyDesired > 0)
	    m_pStandbySlider->setValue( m_StandbyDesired );
        else if (m_Off < m_Standby || m_Off <= m_StandbyDesired )
            m_pStandbySlider->setValue(m_Off);
	if (m_Off == 0 && m_SuspendDesired > 0)
	    m_pSuspendSlider->setValue( m_SuspendDesired );
        else if (m_Off < m_Suspend || m_Off <= m_SuspendDesired )
            m_pSuspendSlider->setValue(m_Off);
	m_bMaintainSanity = true;
    }

    m_bChanged = true;
    emit changed(true);
}

void KEnergy::openUrl(const QString &URL)
{
      new KRun(KUrl( URL ),this);
}

#include "energy.moc"
