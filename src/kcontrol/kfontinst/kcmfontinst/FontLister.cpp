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

#include "FontLister.h"
#include "Misc.h"
#include "KfiConstants.h"
#include <KDE/OrgKdeKDirNotifyInterface>
#include <QtGui/QApplication>
#include <QtGui/QCursor>

//#define KFI_FONTLISTER_DEBUG

#ifdef KFI_FONTLISTER_DEBUG
#include <KDE/KDebug>
#endif

namespace KFI
{

CFontLister::CFontLister(QObject *parent)
           : QObject(parent),
             itsListing(LIST_ALL),
             itsAutoUpdate(true),
             itsUpdateRequired(false),
             itsJob(NULL),
             itsJobSize(0)
{
    org::kde::KDirNotify *kdirnotify = new org::kde::KDirNotify(QString(), QString(),
                                                                QDBusConnection::sessionBus(), this);
    connect(kdirnotify, SIGNAL(FileRenamed(QString,QString)), SLOT(fileRenamed(QString,QString)));
    connect(kdirnotify, SIGNAL(FilesAdded(QString)), SLOT(filesAdded(QString)));
    connect(kdirnotify, SIGNAL(FilesRemoved(QStringList)), SLOT(filesRemoved(QStringList)));
}

void CFontLister::scan(const KUrl &url)
{
    if(!busy())
    {
        if(itsItemsToRename.count())
        {
            emit renameItems(itsItemsToRename);
            itsItemsToRename.clear();
        }

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        itsUpdateRequired=false;

        itsListing=LIST_ALL;
        if(Misc::root())
            itsJob=KIO::listDir(KUrl(KFI_KIO_FONTS_PROTOCOL":/"), KIO::HideProgressInfo);
        else if(url.isEmpty())
            itsJob=KIO::listDir(KUrl(KFI_KIO_FONTS_PROTOCOL":/"KFI_KIO_FONTS_ALL), KIO::HideProgressInfo);
        else
        {
            itsListing=listing(url);
            itsJob=KIO::listDir(url, KIO::HideProgressInfo);
        }

        emit started();
        connect(itsJob, SIGNAL(entries(KIO::Job *, const KIO::UDSEntryList &)), this,
                SLOT(entries(KIO::Job *, const KIO::UDSEntryList &)));
        connect(itsJob, SIGNAL(infoMessage(KJob *, const QString&, const QString& )),
                SLOT(infoMessage(KJob *, const QString &)));
        connect(itsJob, SIGNAL(result(KJob *)), this, SLOT(result(KJob *)));
        //connect(itsJob, SIGNAL(percent(KJob *, unsigned long)), this,
        //        SLOT(percent(KJob *, unsigned long)));
        connect(itsJob, SIGNAL(totalSize(KJob *, qulonglong)), this,
                SLOT(totalSize(KJob *, qulonglong)));
        connect(itsJob, SIGNAL(processedSize(KJob *, qulonglong)), this,
                SLOT(processedSize(KJob *, qulonglong)));
    }
}

void CFontLister::setAutoUpdate(bool on)
{
    if(itsAutoUpdate!=on)
    {
        itsAutoUpdate=on;
        if(on && itsUpdateRequired)
        {
            if(itsItemsToRename.count())
            {
                emit renameItems(itsItemsToRename);
                itsItemsToRename.clear();
            }
            itsUpdateRequired=false;
            scan();
        }
    }
}

void CFontLister::fileRenamed(const QString &from, const QString &to)
{
    KUrl       fromU(from);
    RenameList rename;

    if(KFI_KIO_FONTS_PROTOCOL==fromU.protocol())
    {
        ItemCont::Iterator it(itsItems.find(fromU));

#ifdef KFI_FONTLISTER_DEBUG
        kDebug() << "from: " << from << " to: " << to;
#endif

        if(it!=itsItems.end())
        {
            KFileItem item(*it);
            KUrl      toU(to);

            item.setUrl(toU);
            itsItems.erase(it);

            if(itsItems.contains(toU))
            {
                KFileItemList items;

                items.append(item);
                removeItems(items);
            }
            else
            {
                itsItems[toU]=item;
                rename.append(Rename(fromU, toU));
            }
        }
#ifdef KFI_FONTLISTER_DEBUG
        else
            kDebug() << "Item not found???";
#endif
    }

    if(rename.count())
        if(itsAutoUpdate)
            emit renameItems(rename);
        else
        {
            itsUpdateRequired=true;
            itsItemsToRename+=rename;
        }
}

void CFontLister::filesAdded(const QString &dir)
{
#ifdef KFI_FONTLISTER_DEBUG
    kDebug() <<  dir;
#endif
    KUrl url(dir);

    if(KFI_KIO_FONTS_PROTOCOL==url.protocol())
        if(itsAutoUpdate)
            scan(url);
        else
            itsUpdateRequired=true;
}

void CFontLister::filesRemoved(const QStringList &files)
{
    QStringList::ConstIterator it(files.begin()),
                               end(files.end());

#ifdef KFI_FONTLISTER_DEBUG
    kDebug() << files.count();
#endif
    for(; it!=end; ++it)
    {
        KUrl          url(*it);
        KFileItemList itemsToRemove;

        if(KFI_KIO_FONTS_PROTOCOL==url.protocol())
        {
            ItemCont::Iterator it(itsItems.find(url));

            if(it!=itsItems.end())
            {
                KFileItem item(*it);
#ifdef KFI_FONTLISTER_DEBUG
                kDebug() << "Delete : " << item.url().prettyUrl();
#endif
                itemsToRemove.append(item);
                itsItems.erase(it);
            }
        }

        removeItems(itemsToRemove);
    }
}

void CFontLister::result(KJob *job)
{
    itsJob=NULL;
#ifdef KFI_FONTLISTER_DEBUG
    kDebug();
#endif
    if(job && !job->error())
    {
        KFileItemList      itemsToRemove;
        ItemCont::Iterator it(itsItems.begin());

        while(it!=itsItems.end())
            if((*it).isMarked())
            {
#ifdef KFI_FONTLISTER_DEBUG
                kDebug() << (*it).url().prettyUrl() << " IS MARKED";
#endif
                (*it).unmark();
                ++it;
            }
            else if(inScope((*it).url()))
            {
                ItemCont::Iterator remove(it);
                KFileItem          item(*it);

#ifdef KFI_FONTLISTER_DEBUG
                kDebug() << (*it).url().prettyUrl() << " IS **NOT** MARKED";
#endif

                itemsToRemove.append(item);
                ++it;
                itsItems.erase(remove);
            }
            else
            {
#ifdef KFI_FONTLISTER_DEBUG
                kDebug() << (*it).url().prettyUrl() << " IS NOT IN SCOPE";
#endif
                ++it;
            }

        removeItems(itemsToRemove);
    }
    else
    {
#ifdef KFI_FONTLISTER_DEBUG
        kDebug() << "Error :-(";
#endif
        ItemCont::Iterator it(itsItems.begin()),
                           end(itsItems.end());

        for(; it!=end; ++it)
            (*it).unmark();
    }

    QApplication::restoreOverrideCursor();
    emit completed();
}

void CFontLister::entries(KIO::Job *, const KIO::UDSEntryList &entries)
{
    KIO::UDSEntryList::ConstIterator it(entries.begin()),
                                     end(entries.end());
    KFileItemList                    newFonts;

    for(; it!=end; ++it)
    {
        const QString name((*it).stringValue(KIO::UDSEntry::UDS_NAME));

        if(!name.isEmpty() && name!="." && name!="..")
        {
            KUrl url((*it).stringValue(KIO::UDSEntry::UDS_URL));

            if(!itsItems.contains(url))
            {
                KFileItem item(*it, url);

#ifdef KFI_FONTLISTER_DEBUG
                kDebug() << "New item:" << item.url().prettyUrl();
#endif
                itsItems[url]=item;
                newFonts.append(item);
            }
            itsItems[url].mark();
#ifdef KFI_FONTLISTER_DEBUG
            kDebug() << "Marking:" << itsItems[url].url().prettyUrl() << " [" << url << ']';
#endif
        }
    }

    if(newFonts.count())
    {
#ifdef KFI_FONTLISTER_DEBUG
        kDebug() << "Have " << newFonts.count() << " new fonts";
#endif
        emit newItems(newFonts);
    }
}

/*
void CFontLister::percent(KJob *job, unsigned long p)
{
    emit percent(p);
}
*/

void CFontLister::processedSize(KJob *, qulonglong s)
{
    emit percent(itsJobSize>0 ? (s*100)/itsJobSize : 100);
}

void CFontLister::totalSize(KJob *, qulonglong s)
{
    itsJobSize=s;
}

void CFontLister::infoMessage(KJob *, const QString &msg)
{
    emit message(msg);
}

void CFontLister::removeItems(KFileItemList &items)
{
    emit deleteItems(items);
}
#ifndef KDE_USE_FINAL
inline bool isSysFolder(const QString &sect)
{
    return i18n(KFI_KIO_FONTS_SYS)==sect || KFI_KIO_FONTS_SYS==sect;
}
#endif

inline bool isUserFolder(const QString &sect)
{
    return i18n(KFI_KIO_FONTS_USER)==sect || KFI_KIO_FONTS_USER==sect;
}

CFontLister::EListing CFontLister::listing(const KUrl &url)
{
    QString path(url.path(KUrl::RemoveTrailingSlash).section('/', 1, 1));

    if(isSysFolder(path))
        return LIST_SYS;
    else if(isUserFolder(path))
        return LIST_USER;
    return LIST_ALL;
}

bool CFontLister::inScope(const KUrl &url)
{
    switch(itsListing)
    {
        default:
        case LIST_ALL:
            return true;
        case LIST_SYS:
            return isSysFolder(url.path(KUrl::RemoveTrailingSlash).section('/', 1, 1));
        case LIST_USER:
            return isUserFolder(url.path(KUrl::RemoveTrailingSlash).section('/', 1, 1));
    }
}

}

#include "FontLister.moc"
