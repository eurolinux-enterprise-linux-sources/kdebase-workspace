/* This file is part of the Nepomuk Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "queryclientwrapper.h"
#include "nepomuksearchrunner.h"
#include "queryserviceclient.h"
#include "result.h"
#include "query.h"
#include "queryparser.h"

#include <Nepomuk/Resource>
#include <Nepomuk/Types/Class>

#include <Soprano/Vocabulary/Xesam>

#include <KIcon>
#include <KDebug>
#include <KMimeType>

#include <Plasma/QueryMatch>
#include <Plasma/RunnerContext>
#include <Plasma/AbstractRunner>

#include <QtCore/QTimer>
#include <QtCore/QMutex>


Q_DECLARE_METATYPE(Nepomuk::Resource)

static const int s_maxResults = 10;

Nepomuk::QueryClientWrapper::QueryClientWrapper(SearchRunner* runner, Plasma::RunnerContext* context)
    : QObject(),
      m_runner(runner),
      m_runnerContext(context)
{
    // initialize the query client
    m_queryServiceClient = new Nepomuk::Search::QueryServiceClient(this);
    connect(m_queryServiceClient, SIGNAL(newEntries(const QList<Nepomuk::Search::Result>&)),
             this, SLOT(slotNewEntries(const QList<Nepomuk::Search::Result>&)));
}


Nepomuk::QueryClientWrapper::~QueryClientWrapper()
{
}


void Nepomuk::QueryClientWrapper::runQuery()
{
    //kDebug() << m_runnerContext->query();

    // add a timeout in case something goes wrong (no user wants to wait more than 30 seconds)
    QTimer::singleShot(30000, m_queryServiceClient, SLOT(close()));

    Search::Query q = Search::QueryParser::parseQuery(m_runnerContext->query());
    q.setLimit(s_maxResults);
    m_queryServiceClient->blockingQuery(q);

    //kDebug() << m_runnerContext->query() << "done";
}


namespace {
qreal normalizeScore(double score) {
    // no search result is ever a perfect match, NEVER. And mostly, when typing a URL
    // the users wants to open that url instead of using the search result. Thus, all
    // search results need to have a lower score than URLs which can drop to 0.5
    // And in the end, for 10 results, the score is not that important at the moment.
    // This can be improved in the future.
    // We go the easy way here and simply cut the score at 0.4
    return qMin(0.4, score);
}
}

void Nepomuk::QueryClientWrapper::slotNewEntries(const QList<Nepomuk::Search::Result>& results)
{
    foreach(const Search::Result& result, results) {
        Plasma::QueryMatch match(m_runner);
        match.setType(Plasma::QueryMatch::PossibleMatch);
        match.setRelevance(normalizeScore(result.score()));

        Nepomuk::Resource res(result.resourceUri());

        QString type;
        if (res.hasType(Soprano::Vocabulary::Xesam::File()) ||
            res.resourceUri().scheme() == "file") {
            type = KMimeType::findByUrl(res.resourceUri())->comment();
        } else {
            type = Nepomuk::Types::Class(res.resourceType()).label();
        }

        match.setText(i18nc("@action file/resource to be opened from KRunner. %1 is the name and %2 the type",
                            "Open %1 (%2)",
                            res.genericLabel(),
                            type));
        QString s = res.genericIcon();
        match.setIcon(KIcon(s.isEmpty() ? QString("nepomuk") : s));

        match.setData(qVariantFromValue(res));
        match.setId(res.resourceUri().toString());
        m_runnerContext->addMatch(m_runnerContext->query(), match);
    }
}

#include "queryclientwrapper.moc"
