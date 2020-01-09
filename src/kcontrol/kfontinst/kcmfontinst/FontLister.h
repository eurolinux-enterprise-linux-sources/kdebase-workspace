#ifndef __FONT_LISTER_H__
#define __FONT_LISTER_H__

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

class KJob;
class QString;

#include <KDE/KIO/Job>
#include <KDE/KFileItem>
#include <QtCore/QObject>
#include <QtCore/QHash>

namespace KFI
{

struct Rename
{
    Rename(const KUrl &f, const KUrl &t)
        : from(f), to(t) { }

    KUrl from,
         to;
};

typedef QList<Rename> RenameList;

class CFontLister : public QObject
{
    Q_OBJECT

    private:

    enum EListing
    {
        LIST_ALL,
        LIST_USER,
        LIST_SYS
    };

    public:


    CFontLister(QObject *parent);

    void scan(const KUrl &url=KUrl());
    void setAutoUpdate(bool e);
    bool busy() const { return NULL!=itsJob; }

    Q_SIGNALS:

    void newItems(const KFileItemList &items);
    void deleteItems(const KFileItemList &items);
    void renameItems(const RenameList &items);
    void started();
    void completed();
    void percent(int);
    void message(const QString &msg);

    private Q_SLOTS:

    void fileRenamed(const QString &from, const QString &to);
    void filesAdded(const QString &dir);
    void filesRemoved(const QStringList &files);
    void result(KJob *job);
    void entries(KIO::Job *job, const KIO::UDSEntryList &entries);
    void processedSize(KJob *job, qulonglong s);
    void totalSize(KJob *job, qulonglong s);
    void infoMessage(KJob *job, const QString &msg);

    private:

    void removeItems(KFileItemList &items);
    bool inScope(const KUrl &url);

    static EListing listing(const KUrl &url);

    private:

    typedef QHash<KUrl, KFileItem> ItemCont;

    EListing   itsListing;
    ItemCont   itsItems;
    bool       itsAutoUpdate,
               itsUpdateRequired;
    KIO::Job   *itsJob;
    qulonglong itsJobSize;
    RenameList itsItemsToRename;
};

}

#endif
