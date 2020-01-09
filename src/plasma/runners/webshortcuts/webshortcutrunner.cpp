/*
 *   Copyright (C) 2007 Teemu Rytilahti <tpr@iki.fi>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
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

#include "webshortcutrunner.h"

#include <QStringList>

#include <KDebug>
#include <KRun>
#include <KLocale>
#include <KMimeType>
#include <KStandardDirs>
#include <KToolInvocation>
#include <KUrl>

WebshortcutRunner::WebshortcutRunner(QObject *parent, const QVariantList& args)
    : Plasma::AbstractRunner(parent, args),
      m_match(this)
{
    Q_UNUSED(args);
    setObjectName("Web Shortcut");
    // query ktrader for all available searchproviders and preload the default icon
    m_icon = KIcon("internet-web-browser");
    loadDelimiter();
    setIgnoredTypes(Plasma::RunnerContext::FileSystem);

    m_match.setType(Plasma::QueryMatch::ExactMatch);
    m_match.setRelevance(0.9);
}

void WebshortcutRunner::loadDelimiter()
{
    // TODO: KDirWatch :)
    KConfig kuriconfig("kuriikwsfilterrc", KConfig::NoGlobals);
    KConfigGroup generalgroup(&kuriconfig, "General");
    m_delimiter = generalgroup.readEntry("KeywordDelimiter", QString(':'));
    //kDebug() << "keyworddelimiter is: " << delimiter;

    QList<Plasma::RunnerSyntax> syns;

    const KService::List offers = serviceQuery("SearchProvider");
    if (!offers.isEmpty()) {
        QString knownShortcuts;

        foreach (const KService::Ptr &offer, offers) {
            knownShortcuts.append("<li>");

            if (offer->comment().isEmpty()) {
                knownShortcuts.append(i18nc("A web shortcut and its name",
                                            "%1: %2",
                                            offer->property("Keys", QVariant::String).toString(),
                                            offer->name()));
            } else {
                knownShortcuts.append(i18nc("A web shortcut, its name and a description",
                                            "%1: %2 %3",
                                            offer->property("Keys", QVariant::String).toString(),
                                            offer->name(),
                                            offer->comment()));
            }

            knownShortcuts.append("</li>");
        }

        Plasma::RunnerSyntax s("shortcut" + m_delimiter + ":q:",
                 i18n("Opens the location associated with \"shortcut\"  in a web browser with the query :q:. "
                      "Known shortcuts include: <ul>%1<ul>", knownShortcuts));
        syns << s;
    }

    setSyntaxes(syns);
}

WebshortcutRunner::~WebshortcutRunner()
{
}

void WebshortcutRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();

    if (term.length() < 3 || !term.contains(m_delimiter)) {
        return;
    }

    //kDebug() << "checking with" << term;

    int delimIndex = term.indexOf(m_delimiter);

    if (delimIndex == term.length() - 1) {
        return;
    }

    QString key = term.left(delimIndex);

    if (key == m_lastFailedKey) {
        // we already know it's going to suck ;)
        return;
    }

    if (key != m_lastKey) {
        const KService::List offers = serviceQuery("SearchProvider", QString("'%1' in Keys").arg(key));

        if (!context.isValid()) {
            return;
        }

        if (offers.isEmpty()) {
            m_lastFailedKey = key;
            return;
        }

        KService::Ptr service = offers.at(0);
        m_lastKey = key;
        m_lastFailedKey.clear();
        m_lastServiceName = service->name();

        const QString query = service->property("Query").toString();
        m_match.setData(query);

        if (service->icon().isEmpty()) {
            m_match.setIcon(iconForUrl(query));
        } else {
            m_match.setIcon(KIcon(service->icon()));
        }
    }

    QString actionText = i18n("Search %1 for %2", m_lastServiceName,
                              term.right(term.length() - delimIndex - 1));
    //kDebug() << "url is" << url << "!!!!!!!!!!!!!!!!!!!!!!!";

    m_match.setText(actionText);
    context.addMatch(term, m_match);
}

QString WebshortcutRunner::searchQuery(const QString &query, const QString &term)
{
    const QString searchWord = term.right(term.length() - term.indexOf(m_delimiter) - 1);
    if (searchWord.isEmpty()) {
        return QString();
    }

    QString finalQuery(query);
    // FIXME? currently only basic searches are supported
    finalQuery.replace("\\{@}", searchWord);
    KUrl url(finalQuery);
    return url.url();
}

KIcon WebshortcutRunner::iconForUrl(const KUrl &url)
{
    // locate the favicon
    const QString iconFile = KMimeType::favIconForUrl(url);

    if (iconFile.isEmpty()) {
        return m_icon;
    }

    m_lastIcon = KIcon(iconFile);
    return m_lastIcon;
}

void WebshortcutRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    QString location = searchQuery(match.data().toString(), context.query());

    if (!location.isEmpty()) {
        KToolInvocation::invokeBrowser(location);
    }
}

#include "webshortcutrunner.moc"
