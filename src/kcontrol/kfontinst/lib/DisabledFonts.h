#ifndef __DISABLED_FONTS_H__
#define __DISABLED_FONTS_H__

/*
 * KFontInst - KDE Font Installer
 *
 * Copyright 2003-2007 Craig Drummond <craig@kde.org>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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


#include <QtCore/QSet>
#include <QtCore/QList>
#include <QtGui/QFontDatabase>
#include <time.h>
#include "KfiConstants.h"
#include "Misc.h"
#include "kfontinst_export.h"
#include <fontconfig/fontconfig.h>

class QDomElement;
class QTextStream;

//
// Class used to store list of disabled fonts, and font groups, within an XML file.
namespace KFI
{

class KFONTINST_EXPORT CDisabledFonts
{
    public:

    struct LangWritingSystemMap
    {
        QFontDatabase::WritingSystem ws;
        const FcChar8                *lang;
    };

    struct TFile
    {
        TFile(const QString &p=QString(), int f=0, const QString &fnd=QString()) : path(p), foundry(fnd), face(f) { }

        bool operator==(const TFile &o) const { return face==o.face && path==o.path; } 
        bool load(QDomElement &elem);

        operator QString() const { return path; }

        QString path,
                foundry;
        int     face;  // This is only really required for TTC fonts -> where a file will belong
    };                 // to more than one font.

    struct KFONTINST_EXPORT TFileList : public QList<TFile>
    {
        Iterator locate(const TFile &t) { int i = indexOf(t); return (-1==i ? end() : (begin()+i)); }
        void     add(const TFile &t) const { (const_cast<TFileList *>(this))->append(t); }

        QString  toString(bool skipFirst=false) const;
        void     fromString(QString &s);
    };

    struct KFONTINST_EXPORT TFont : public Misc::TFont
    {
        TFont(const QString &f=QString(), unsigned long s=KFI_NO_STYLE_INFO, qulonglong ws=0) : Misc::TFont(f, s), writingSystems(ws) { }
        TFont(const Misc::TFont &f) : Misc::TFont(f), writingSystems(0) { }

        bool operator==(const TFont &o) const { return styleInfo==o.styleInfo && family==o.family; }
        bool load(QDomElement &elem, bool &modified);

        const QString & getName() const;

        mutable QString name;
        TFileList       files;
        qulonglong      writingSystems;
    };

    struct KFONTINST_EXPORT TFontList : public QSet<TFont>
    {
        Iterator locate(const TFont &t);
        Iterator locate(const Misc::TFont &t);
        void     add(const TFont &t) const;
    };

    explicit CDisabledFonts(bool sys=false);
    ~CDisabledFonts()          { save(); }

    static const LangWritingSystemMap * languageForWritingSystemMap() { return theirLanguageForWritingSystem; }

    //
    // Refresh checks the timestamp of the file to determine if changes have been
    // made elsewhere.
    void reload();
    bool refresh();
    void load(bool lock=true);
    bool save();
    bool modifiable() const    { return itsModifiable; }
    bool modified() const      { return itsModified; }

    bool disable(const TFont &font);
    bool enable(const QString &family, unsigned long styleInfo)
             { return enable(TFont(family, styleInfo)); }
    bool enable(const TFont &font)
             { return enable(itsFonts.locate(font)); }
    bool enable(TFontList::Iterator font);

    TFontList::Iterator find(const QString &name, int face);
    void                remove(TFontList::Iterator it)  { itsFonts.erase(it);
                                                          itsModified=true; }
    TFontList & items() { return itsFonts; }

    private:

    CDisabledFonts(const CDisabledFonts &o);
    void merge(const CDisabledFonts &other);

    static void createWritingSystemMap();

    private:

    QString   itsFileName;
    time_t    itsTimeStamp;
    bool      itsModified,
              itsModifiable;
    TFontList itsFonts;
    int       itsMods;

    static const LangWritingSystemMap theirLanguageForWritingSystem[];
};

inline KDE_EXPORT uint qHash(const CDisabledFonts::TFont &key)
{
    return qHash((Misc::TFont&)key);
}

}

KFONTINST_EXPORT QTextStream & operator<<(QTextStream &s, const KFI::CDisabledFonts::TFile &f);
KFONTINST_EXPORT QTextStream & operator<<(QTextStream &s, const KFI::CDisabledFonts::TFont &f);

#endif
