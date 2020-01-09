/*
 *   Copyright 2007 Glenn Ergeerts <glenn.ergeerts@telenet.be>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "bookmarksrunner.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QLabel>
#include <QList>
#include <QStack>
#include <QWidget>

#include <KIcon>
#include <KBookmarkManager>
#include <KToolInvocation>
#include <KUrl>
#include <KStandardDirs>


BookmarksRunner::BookmarksRunner( QObject* parent, const QVariantList &args )
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args)
    setObjectName("Bookmarks");
    m_icon = KIcon("bookmarks");
    m_bookmarkManager = KBookmarkManager::userBookmarksManager();
    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Finds web browser bookmarks matching :q:.")));
}

BookmarksRunner::~BookmarksRunner()
{
}

void BookmarksRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    if (term.length() < 3) {
        return;
    }

    KBookmarkGroup bookmarkGroup = m_bookmarkManager->root();

    QList<Plasma::QueryMatch> matches;
    QStack<KBookmarkGroup> groups;

    KBookmark bookmark = bookmarkGroup.first();
    while (!bookmark.isNull()) {
        if (!context.isValid()) {
            return;
        }

        if (bookmark.isGroup()) { // descend
            //kDebug () << "descending into" << bookmark.text();
            groups.push(bookmarkGroup);
            bookmarkGroup = bookmark.toGroup();
            bookmark = bookmarkGroup.first();

            while (bookmark.isNull() && !groups.isEmpty()) {
                if (!context.isValid()) {
                    return;
                }

                bookmark = bookmarkGroup;
                bookmarkGroup = groups.pop();
                bookmark = bookmarkGroup.next(bookmark);
            }

            continue;
        }

        Plasma::QueryMatch::Type type = Plasma::QueryMatch::NoMatch;
        qreal relevance = 0;

        if (bookmark.text().toLower() == term.toLower()) {
            type = Plasma::QueryMatch::ExactMatch;
            relevance = 1.0;
        } else if (bookmark.text().contains(term, Qt::CaseInsensitive)) {
            type = Plasma::QueryMatch::PossibleMatch;
            relevance = 0.45;
        } else if (bookmark.url().prettyUrl().contains(term, Qt::CaseInsensitive)) {
            type = Plasma::QueryMatch::PossibleMatch;
            relevance = 0.2;
        }

        if (type != Plasma::QueryMatch::NoMatch) {
            //kDebug() << "Found bookmark: " << bookmark.text() << " (" << bookmark.url().prettyUrl() << ")";
            // getting the favicon is too slow and can easily lead to starving the thread pool out
            /*
            QIcon icon = getFavicon(bookmark.url());
            if (icon.isNull()) {
                match->setIcon(m_icon);
            }
            else {
                match->setIcon(icon);
            }
            */
            Plasma::QueryMatch match(this);
            match.setType(type);
            match.setRelevance(relevance);
            match.setIcon(m_icon);
            match.setText(bookmark.text());
            match.setData(bookmark.url().url());
            matches << match;
        }

        bookmark = bookmarkGroup.next(bookmark);
        while (bookmark.isNull() && !groups.isEmpty()) {
            if (!context.isValid()) {
                return;
            }

            bookmark = bookmarkGroup;
            bookmarkGroup = groups.pop();
            //kDebug() << "ascending from" << bookmark.text() << "to" << bookmarkGroup.text();
            bookmark = bookmarkGroup.next(bookmark);
        }
    }

    context.addMatches(term, matches);
}

KIcon BookmarksRunner::getFavicon(const KUrl &url)
{
    // query the favicons module
    QDBusInterface favicon("org.kde.kded", "/modules/favicons", "org.kde.FavIcon");
    QDBusReply<QString> reply = favicon.call("iconForUrl", url.url());

    if (!reply.isValid()) {
        return KIcon();
    }

    // locate the favicon
    const QString iconFile = KGlobal::dirs()->findResource("cache",reply.value()+".png");
    if(iconFile.isNull()) {
        return KIcon();
    }

    KIcon icon = KIcon(iconFile);

    return icon;
}

void BookmarksRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &action)
{
    Q_UNUSED(context);
    KUrl url = (KUrl)action.data().toString();
    //kDebug() << "BookmarksRunner::run opening: " << url.url();
    KToolInvocation::invokeBrowser(url.url());
}

#include "bookmarksrunner.moc"
