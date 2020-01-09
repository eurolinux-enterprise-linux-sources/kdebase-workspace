/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
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

#include "servicerunner.h"

#include <QWidget>
#include <QMutexLocker>

#include <KIcon>
#include <KDebug>
#include <KLocale>
#include <KRun>
#include <KService>
#include <KServiceTypeTrader>

ServiceRunner::ServiceRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args)

    setObjectName("Application");
    setPriority(AbstractRunner::HighestPriority);

    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Finds applications whose name or description match :q:")));
}

ServiceRunner::~ServiceRunner()
{
}

void ServiceRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    if (term.length() <  3) {
        return;
    }


    // Search for applications which are executable and case-insensitively match the search term
    // See http://techbase.kde.org/Development/Tutorials/Services/Traders#The_KTrader_Query_Language
    // if the following is unclear to you.
    QString query = QString("exist Exec and ('%1' =~ Name)").arg(term);

    KService::List services = KServiceTypeTrader::self()->query("Application", query);

    QList<Plasma::QueryMatch> matches;

    QHash<QString, bool> seen;
    if (!services.isEmpty()) {
        //kDebug() << service->name() << "is an exact match!" << service->storageId() << service->exec();
        foreach (const KService::Ptr &service, services) {
            if (!service->noDisplay()) {
                Plasma::QueryMatch match(this);
                match.setType(Plasma::QueryMatch::ExactMatch);
                setupMatch(service, match);
                match.setRelevance(1);
                matches << match;
                seen[service->storageId()] = true;
                seen[service->exec()] = true;
            }
        }
    }

    if (!context.isValid()) {
        return;
    }

    // Search for applications which are executable and the term case-insensitive matches any of
    // * a substring of one of the keywords
    // * a substring of the GenericName field
    // * a substring of the Name field
    // Note that before asking for the content of e.g. Keywords and GenericName we need to ask if
    // they exist to prevent a tree evaluation error if they are not defined.
    query = QString("exist Exec and ( (exist Keywords and '%1' ~subin Keywords) or (exist GenericName and '%1' ~~ GenericName) or (exist Name and '%1' ~~ Name) )").arg(term);
    services = KServiceTypeTrader::self()->query("Application", query);
    services += KServiceTypeTrader::self()->query("KCModule", query);

    //kDebug() << "got " << services.count() << " services from " << query;
    foreach (const KService::Ptr &service, services) {
        if (!context.isValid()) {
            return;
        }

        if (service->noDisplay()) {
            continue;
        }

        QString id = service->storageId();
        QString exec = service->exec();
        if (seen.contains(id) || seen.contains(exec)) {
            //kDebug() << "already seen" << id << exec;
            continue;
        }

        //kDebug() << "haven't seen" << id << "so processing now";
        seen[id] = true;
        seen[exec] = true;

        Plasma::QueryMatch match(this);
        match.setType(Plasma::QueryMatch::PossibleMatch);
        setupMatch(service, match);
        qreal relevance(0.6);

        if (service->name().contains(term, Qt::CaseInsensitive)) {
            relevance = 0.8;

            if (service->name().startsWith(term, Qt::CaseInsensitive)) {
                relevance += 0.1;
            }
        } else if (service->genericName().contains(term, Qt::CaseInsensitive)) {
            relevance = 0.7;

            if (service->genericName().startsWith(term, Qt::CaseInsensitive)) {
                relevance += 0.1;
            }
        }

        if (service->categories().contains("KDE") || service->serviceTypes().contains("KCModule")) {
            //kDebug() << "found a kde thing" << id << match.subtext() << relevance;
            if (id.startsWith("kde-")) {
                // This is an older version, let's disambiguate it
                QString subtext("KDE3");

                //kDebug() << "old" << service->type();
                if (service->type() == "KCModule") {
                    // avoid showing old kcms
                    continue;
                }

                if (!match.subtext().isEmpty()) {
                    subtext.append(", " + match.subtext());
                }

                match.setSubtext(subtext);
            } else {
                relevance += .1;
            }
        }

        //kDebug() << service->name() << "is this relevant:" << relevance;
        match.setRelevance(relevance);
        matches << match;
    }

    context.addMatches(term, matches);
}

void ServiceRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);
    KService::Ptr service = KService::serviceByStorageId(match.data().toString());
    if (service) {
        KRun::run(*service, KUrl::List(), 0);
    }
}

void ServiceRunner::setupMatch(const KService::Ptr &service, Plasma::QueryMatch &match)
{
    const QString name = service->name();

    match.setText(name);
    match.setData(service->storageId());

    if (!service->genericName().isEmpty() && service->genericName() != name) {
        match.setSubtext(service->genericName());
    } else if (!service->comment().isEmpty()) {
        match.setSubtext(service->comment());
    }

    if (!service->icon().isEmpty()) {
        match.setIcon(KIcon(service->icon()));
    }
}

#include "servicerunner.moc"

