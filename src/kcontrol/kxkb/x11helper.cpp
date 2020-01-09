/*
 *  Copyright (C) 2006 Andriy Rysin (rysin@kde.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <QDir>
#include <QRegExp>
#include <QTextStream>
#include <QX11Info>

#include <kdebug.h>

#ifndef HAVE_XKLAVIER
#include <kglobal.h>
#include <klocale.h>
#endif


#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define explicit int_explicit        // avoid compiler name clash in XKBlib.h
#include <X11/XKBlib.h>
#undef explicit
#include <X11/extensions/XKBrules.h>

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

#include "kxkbconfig.h"

#include "x11helper.h"
#include <config-workspace.h>

#ifndef HAVE_XKLAVIER

// Compiler will size array automatically.
static const char* const X11DirList[] =
    {
        XLIBDIR,
        "/etc/X11/",
        "/usr/share/X11/",
        "/usr/local/share/X11/",
        "/usr/X11R6/lib/X11/",
        "/usr/X11R6/lib64/X11/",
        "/usr/local/X11R6/lib/X11/",
        "/usr/local/X11R6/lib64/X11/",
        "/usr/lib/X11/",
        "/usr/lib64/X11/",
        "/usr/local/lib/X11/",
        "/usr/local/lib64/X11/",
        "/usr/pkg/share/X11/",
        "/usr/pkg/xorg/lib/X11/"
    };

// Compiler will size array automatically.
static const char* const rulesFileList[] =
    {
	"xkb/rules/base",
	"xkb/rules/xorg",
	"xkb/rules/xfree86"
    };

// Macro will return number of elements in any static array as long as the
// array has at least one element.
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static const int X11_DIR_COUNT = ARRAY_SIZE(X11DirList);
static const int X11_RULES_COUNT = ARRAY_SIZE(rulesFileList);

const QString
X11Helper::findX11Dir()
{
	for(int ii=0; ii<X11_DIR_COUNT; ii++) {
		const char* xDir = X11DirList[ii];
		if( xDir != NULL && QDir(QString(xDir) + "xkb").exists() ) {
//			for(int jj=0; jj<X11_RULES_COUNT; jj++) {
//
//			}
			return QString(xDir);
	    }

//	  if( X11_DIR.isEmpty() ) {
//	    return;
//	  }
	}
	return NULL;
}

const QString
X11Helper::findXkbRulesFile(const QString &x11Dir, Display *dpy)
{
	QString rulesFile;
	XkbRF_VarDefsRec vd;
	char *tmp = NULL;

	if (XkbRF_GetNamesProp(dpy, &tmp, &vd) && tmp != NULL ) {
// 			kDebug() << "namesprop " << tmp ;
	  	rulesFile = x11Dir + QString("xkb/rules/%1").arg(tmp);
// 			kDebug() << "rulesF " << rulesFile ;
	}
	else {
    // old way
    	for(int ii=0; ii<X11_RULES_COUNT; ii++) {
    		const char* ruleFile = rulesFileList[ii];
     		QString xruleFilePath = x11Dir + ruleFile;
  			kDebug() << "trying xrules path " << xruleFilePath;
		    if( QFile(xruleFilePath).exists() ) {
				rulesFile = xruleFilePath;
			break;
		    }
    	}
    }

	return rulesFile;
}

RulesInfo*
X11Helper::loadRules(const QString& file, bool layoutsOnly)
{
	XkbRF_RulesPtr xkbRules = XkbRF_Load(QFile::encodeName(file).data(), (char*)"", true, true);

	if (xkbRules == NULL) {
// throw Exception
		return NULL;
	}

    // try to translate layout names by countries in desktop_kdebase
    // this is poor man's translation as it's good only for layout names and only those which match country names
    KGlobal::locale()->insertCatalog("desktop_kdebase");

	RulesInfo* rulesInfo = new RulesInfo();

	for (int i = 0; i < xkbRules->layouts.num_desc; ++i) {
		QString layoutName(xkbRules->layouts.desc[i].name);
		rulesInfo->layouts.insert( layoutName, i18nc("Name", xkbRules->layouts.desc[i].desc) );
	}

	if( layoutsOnly == true ) {
		XkbRF_Free(xkbRules, true);
		return rulesInfo;
	}

//    for (int i = 0; i < xkbRules->variants.num_desc; ++i) {
//        kDebug() << "var:" << xkbRules->variants.desc[i].name << "@" << xkbRules->variants.desc[i].name;
//        if( xkbRules->variants.desc[i].
//        rulesInfo->variants.insert(xkbRules->models.desc[i].name, QString( xkbRules->models.desc[i].desc ) );
//    }

    for (int i = 0; i < xkbRules->models.num_desc; ++i)
        rulesInfo->models.insert(xkbRules->models.desc[i].name, QString( xkbRules->models.desc[i].desc ) );

    for (int i = 0; i < xkbRules->options.num_desc; ++i) {
        QString optionName = xkbRules->options.desc[i].name;

        int colonPos = optionName.indexOf(':');
        QString groupName = optionName.mid(0, colonPos);


	if( colonPos != -1 ) {
            //kDebug() << " option: " << optionName;

	    if( ! rulesInfo->optionGroups.contains( groupName ) ) {
		rulesInfo->optionGroups.insert(groupName, createMissingGroup(groupName));
                kDebug() << " added missing option group: " << groupName;
            }

	    XkbOption option;
	    option.name = optionName;
	    option.description = xkbRules->options.desc[i].desc;
	    option.group = &rulesInfo->optionGroups[ groupName ];

	    rulesInfo->options.insert(optionName, option);
	}
	else {
            if( groupName == "Compose" )
                groupName = "compose";
            if( groupName == "compat" )
                groupName = "numpad";
            
	    XkbOptionGroup optionGroup;
	    optionGroup.name = groupName;
	    optionGroup.description = xkbRules->options.desc[i].desc;
	    optionGroup.exclusive = isGroupExclusive( groupName );

            //kDebug() << " option group: " << groupName;
	    rulesInfo->optionGroups.insert(groupName, optionGroup);
	}
  }

  XkbRF_Free(xkbRules, true);

  return rulesInfo;
}


XkbOptionGroup
X11Helper::createMissingGroup(const QString& groupName)
{
// workaround for empty 'compose' options group description
   XkbOptionGroup optionGroup;
   optionGroup.name = groupName;
//   optionGroup.description = "";
   optionGroup.exclusive = isGroupExclusive( groupName );

   return optionGroup;
}

bool
X11Helper::isGroupExclusive(const QString& groupName)
{
    if( groupName == "ctrl" || groupName == "keypad" || groupName == "nbsp"
            || groupName == "kpdl" || groupName == "caps" || groupName == "altwin" )
	return true;

    return false;
}


/* pretty simple algorithm - reads the layout file and
    tries to find "xkb_symbols"
    also checks whether previous line contains "hidden" to skip it
*/

QList<XkbVariant>*
X11Helper::getVariants(const QString& layout, const QString& x11Dir)
{
  QList<XkbVariant>* result = new QList<XkbVariant>();

  QString file = x11Dir + "xkb/symbols/";
  // workaround for XFree 4.3 new directory for one-group layouts
  if( QDir(file+"pc").exists() )
    file += "pc/";

  file += layout;

//  kDebug() << "reading variants from " << file;

  QFile f(file);
  if (f.open(QIODevice::ReadOnly))
    {
      QTextStream ts(&f);

      QString line;
      QString prev_line;

	  while ( ts.status() == QTextStream::Ok ) {
    	prev_line = line;

		QString str = ts.readLine();
		if( str.isNull() )
		  break;

		line = str.simplified();

	    if (line[0] == '#' || line.left(2) == "//" || line.isEmpty())
		continue;

	    int pos = line.indexOf("xkb_symbols");
	    if (pos < 0)
		continue;

	    if( prev_line.indexOf("hidden") >=0 )
		continue;

	    pos = line.indexOf('"', pos) + 1;
	    int pos2 = line.indexOf('"', pos);
	    if( pos < 0 || pos2 < 0 )
		continue;

            XkbVariant variant;
            variant.name = line.mid(pos, pos2-pos);
            variant.description = line.mid(pos, pos2-pos);
	    result->append(variant);
//  kDebug() << "adding variant " << line.mid(pos, pos2-pos);
      }

      f.close();
    }

    return result;
}

XkbConfig
X11Helper::getGroupNames(Display* dpy)
{
    Atom real_prop_type;
    int fmt;
    unsigned long nitems, extra_bytes;
    char *prop_data = NULL;
    Status ret;
    XkbConfig xkbConfig;

    Atom rules_atom = XInternAtom(dpy, _XKB_RF_NAMES_PROP_ATOM, False);

    /* no such atom! */
    if (rules_atom == None) {       /* property cannot exist */
        kError() << "Failed to fetch layouts from server:" << "could not find the atom" << _XKB_RF_NAMES_PROP_ATOM;
        return xkbConfig;
    }

    ret = XGetWindowProperty(dpy,
                   QX11Info::appRootWindow(),
                   rules_atom, 0L, _XKB_RF_NAMES_PROP_MAXLEN,
                   False, XA_STRING, &real_prop_type, &fmt,
                   &nitems, &extra_bytes,
                   (unsigned char **) (void *) &prop_data);

    /* property not found! */
    if (ret != Success) {
        kError() << "Failed to fetch layouts from server:" << "Could not get the property";
        return xkbConfig;
    }

    /* has to be array of strings */
    if ((extra_bytes > 0) || (real_prop_type != XA_STRING)
                || (fmt != 8)) {
        if (prop_data)
            XFree(prop_data);
        kError() << "Failed to fetch layouts from server:" << "Wrong property format";
        return xkbConfig;
    }

    kDebug() << "prop_data:" << nitems << prop_data;
    QStringList names;
    for(char* p=prop_data; p-prop_data < (long)nitems && p != NULL; p += strlen(p)+1) {
        names.append( p );
        kDebug() << " " << p;
    }

    if( names.count() >= 4 ) { //{ rules, model, layouts, variants, options }
        xkbConfig.model = names[1];
//        kDebug() << "model:" << xkbConfig.model;

        QStringList layouts = names[2].split(KxkbConfig::OPTIONS_SEPARATOR);
        QStringList variants = names[3].split(KxkbConfig::OPTIONS_SEPARATOR);
        
        for(int ii=0; ii<layouts.count(); ii++) {
	    LayoutUnit lu;
	    lu.layout = layouts[ii];
	    lu.variant = ii < variants.count() ? variants[ii] : "";
	    xkbConfig.layouts << lu;
	    kDebug() << "layout nm:" << lu.layout << "variant:" << lu.variant;
        }
        
        if( names.count() >= 5 ) {
            QString options = names[4];
            xkbConfig.options = options.split(KxkbConfig::OPTIONS_SEPARATOR);
            kDebug() << "options:" << options;
        }
    }

    XFree(prop_data);
    return xkbConfig;
}

#endif  /* HAVE_XKLAVIER*/

#ifdef HAVE_XINPUT

int X11Helper::m_xinputEventType = -1;

extern "C" {
    extern int _XiGetDevicePresenceNotifyEvent(Display *);
}

int
X11Helper::isNewDeviceEvent(XEvent* event)
{
    if( m_xinputEventType != -1 && event->type == m_xinputEventType ) {
	XDevicePresenceNotifyEvent *xdpne = (XDevicePresenceNotifyEvent*) event;
	if( xdpne->devchange == DeviceEnabled ) {
	    bool keyboard_device = false;
	    int ndevices;
            XDeviceInfo	*devices = XListInputDevices(xdpne->display, &ndevices);
            if( devices != NULL ) {
                kDebug() << "New device id:" << xdpne->deviceid;
                for(int i=0; i<ndevices; i++) {
                    kDebug() << "id:" << devices[i].id << "name:" << devices[i].name << "used as:" << devices[i].use;
                    if( devices[i].id == xdpne->deviceid 
                            && (devices[i].use == IsXKeyboard || devices[i].use == IsXExtensionKeyboard) ) {
                        keyboard_device = true;
                        break;
                    }
                }
                XFreeDeviceList(devices);
            }
            return keyboard_device;
        }
    }
    return false;
}

int
X11Helper::registerForNewDeviceEvent(Display* display)
{
    int xitype;
    XEventClass xiclass;
    
    DevicePresence(display, xitype, xiclass);
    XSelectExtensionEvent(display, QX11Info::appRootWindow(), &xiclass, 1);
    kDebug() << "Registered for new device events from XInput, class" << xitype;
    m_xinputEventType = xitype;
    return xitype;
}
#else
int
X11Helper::isNewDeviceEvent(XEvent* event)
{
    return false;
}
int
X11Helper::registerForNewDeviceEvent(Display* display)
{
    kWarn() << "Kxkb is compiled without XInput, xkb configuration will be reset when new keyboard device is plugged in!";
    return -1;
}
#endif


const QString X11Helper::X11_WIN_CLASS_ROOT = "<root>";
const QString X11Helper::X11_WIN_CLASS_UNKNOWN = "<unknown>";

QString
X11Helper::getWindowClass(Window winId, Display* dpy)
{
    unsigned long nitems_ret, bytes_after_ret;
    unsigned char* prop_ret;
    Atom     type_ret;
    int      format_ret;
    Window w = (Window)winId;	// suppose WId == Window
    QString  property;

    if( winId == X11Helper::UNKNOWN_WINDOW_ID ) {
        kDebug() << "Got window class for " << winId << ": '" << X11_WIN_CLASS_ROOT << "'";
        return X11_WIN_CLASS_ROOT;
    }

//  kDebug() << "Getting window class for " << winId;
    if((XGetWindowProperty(dpy, w, XA_WM_CLASS, 0L, 256L, 0, XA_STRING,
			&type_ret, &format_ret, &nitems_ret,
			&bytes_after_ret, &prop_ret) == Success) && (type_ret != None)) {
        property = QString::fromLocal8Bit(reinterpret_cast<char*>(prop_ret));
        XFree(prop_ret);
    }
    else {
        property = X11_WIN_CLASS_UNKNOWN;
    }
    kDebug() << "Got window class for " << winId << ": '" << property << "'";

    return property;
}
