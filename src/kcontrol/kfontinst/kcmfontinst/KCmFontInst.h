#ifndef __KCM_FONT_INST_H__
#define __KCM_FONT_INST_H__

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

#include <config-workspace.h>
#include "GroupList.h"
#include "JobRunner.h"
#include <QtCore/QSet>
#include <KDE/KCModule>
#include <KDE/KUrl>
#include <KDE/KConfig>
#include <KDE/KIO/Job>

class KPushButton;
class KProgressDialog;
class KTempDir;
class KZip;
class KToggleAction;
class KActionMenu;
class KAction;
class QProcess;
class QLabel;
class QSplitter;
class QComboBox;

namespace KFI
{

class CFontFilter;
class CFontList;
class CFontPreview;
class CUpdateDialog;
class CFontListView;
class CProgressBar;
class CPreviewSelectAction;

class CKCmFontInst : public KCModule
{
    Q_OBJECT

    public:

    explicit CKCmFontInst(QWidget *parent=NULL, const QVariantList &list=QVariantList());
    virtual ~CKCmFontInst();

    public Q_SLOTS:

    QString quickHelp() const;
    void    fontSelected(const QModelIndex &index, bool en, bool dis);
    void    groupSelected(const QModelIndex &index);
    void    reload();
    void    addFonts();
    void    deleteFonts();
    void    copyFonts();
    void    moveFonts();
    void    enableFonts();
    void    disableFonts();
    void    addGroup();
    void    removeGroup();
    void    enableGroup();
    void    disableGroup();
    void    changeText();
    void    showPreview(bool s);
    void    duplicateFonts();
    void    print();
    void    printGroup();
    void    listingStarted();
    void    listingCompleted();
    void    refreshFontList();
    void    refreshFamilies();
    void    showInfo(const QString &info);
    void    setStatusBar();
    void    addFonts(const QSet<KUrl> &src);
    void    toggleFontManagement(bool on);
    void    selectMode(int mode);
    void    zoomIn();
    void    zoomOut();

    private:

    void    selectGroup(CGroupListItem::EType grp);
    void    print(bool all);
    void    toggleGroup(bool enable);
    void    toggleFonts(bool enable, const QString &grp=QString());
    void    toggleFonts(CJobRunner::ItemList &urls, const QStringList &fonts, bool enable, const QString &grp,
                        bool hasSys);
    bool    working(bool displayMsg=true);
    KUrl    baseUrl(bool sys);
    void    selectMainGroup();
    void    doCmd(CJobRunner::ECommand cmd, const CJobRunner::ItemList &urls, const KUrl &dest);
    CGroupListItem::EType getCurrentGroupType();

    private:

    QWidget              *itsGroupsWidget,
                         *itsFontsWidget,
                         *itsPreviewWidget;
    QComboBox            *itsModeControl;
    QAction              *itsModeAct;
    QSplitter            *itsGroupSplitter,
                         *itsPreviewSplitter;
    CFontPreview         *itsPreview;
    KConfig              itsConfig;
    QLabel               *itsStatusLabel;
    CProgressBar         *itsListingProgress;
    CFontList            *itsFontList;
    CFontListView        *itsFontListView;
    CGroupList           *itsGroupList;
    CGroupListView       *itsGroupListView;
    CPreviewSelectAction *itsPreviewControl;
    KToggleAction        *itsMgtMode,
                         *itsShowPreview;
    KActionMenu          *itsToolsMenu;
    KAction              *itsZoomInAction,
                         *itsZoomOutAction;
    KPushButton          *itsDeleteGroupControl,
                         *itsEnableGroupControl,
                         *itsDisableGroupControl,
                         *itsAddFontControl,
                         *itsDeleteFontControl,
                         *itsEnableFontControl,
                         *itsDisableFontControl;
    CFontFilter          *itsFilter;
    QString              itsLastStatusBarMsg;
    KIO::Job             *itsJob;
    KProgressDialog      *itsProgress;
    CUpdateDialog        *itsUpdateDialog;
    KTempDir             *itsTempDir;
    QProcess             *itsPrintProc;
    KZip                 *itsExportFile;
    QSet<QString>        itsDeletedFonts;
    KUrl::List           itsModifiedUrls;
    CJobRunner           *itsRunner;
};

}

#endif
