/*
 *   Copyright (C) 2006 by Aaron Seigo <aseigo@kde.org>
 *   Copyright (C) 2007-2009 Ryan P. Bitanga <ryan.bitanga@gmail.com>
 *   Copyright (C) 2008 by Davide Bettio <davide.bettio@kdemail.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .
 */

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QLabel>
#include <QToolButton>

#include <KAction>
#include <KStandardGuiItem>
#include <KWindowSystem>

//#include <Plasma/QueryAction>
#include <Plasma/QueryMatch>
#include <Plasma/RunnerManager>
#include <Plasma/Svg>
#include <Plasma/Theme>

#include "qs_dialog.h"
#include "qs_matchview.h"
#include "qs_querymatchitem.h"
#include "qs_queryactionitem.h"

QsDialog::QsDialog(Plasma::RunnerManager *runnerManager, QWidget *parent)
    : KRunnerDialog(runnerManager, parent)
{
    QWidget *w = mainWidget();
    QVBoxLayout *layout = new QVBoxLayout(w);

    QHBoxLayout *hLayout = new QHBoxLayout();

    QToolButton *m_configButton = new QToolButton(w);
    m_configButton->setText(i18n("Settings"));
    m_configButton->setToolTip(i18n("Settings"));
    m_configButton->setIcon(m_iconSvg->pixmap("configure"));
    connect(m_configButton, SIGNAL(clicked()), SLOT(showConfigDialog()));

    QLabel *label = new QLabel(w);
    label->setText("<b>QuickSand</b>");

    QToolButton *m_closeButton = new QToolButton(w);
    KGuiItem guiItem = KStandardGuiItem::close();
    m_closeButton->setText(guiItem.text());
    m_closeButton->setToolTip(guiItem.text().remove('&'));
    m_closeButton->setIcon(m_iconSvg->pixmap("close"));
    connect(m_closeButton, SIGNAL(clicked(bool)), this, SLOT(close()));

    hLayout->addWidget(m_configButton);
    hLayout->addStretch();
    hLayout->addWidget(label);
    hLayout->addStretch();
    hLayout->addWidget(m_closeButton);
    layout->addLayout(hLayout);

    m_matchView = new QuickSand::QsMatchView(w);
    layout->addWidget(m_matchView);
    connect(m_matchView, SIGNAL(textChanged(const QString&)), m_matchView, SLOT(setTitle(const QString&)));

    m_currentMatch = 0;

    m_actionView = new QuickSand::QsMatchView(w);
    layout->addWidget(m_actionView);
    m_actionView->setTitle(i18n("Actions"));
    m_actionView->setItemCountSuffix(i18n("actions"));
    m_actionView->hide();

    Plasma::Theme *theme = Plasma::Theme::defaultTheme();
    QString stylesheet = QString("* {color: %1}").arg(theme->color(Plasma::Theme::TextColor).name());
    setStyleSheet(stylesheet);
    QColor buttonBgColor = theme->color(Plasma::Theme::BackgroundColor);
    QString buttonStyleSheet = QString("QToolButton { border: 1px solid %4; border-radius: 4px; padding: 2px;"
                                        " background-color: rgba(%1, %2, %3, %5); }")
                                        .arg(buttonBgColor.red())
                                        .arg(buttonBgColor.green())
                                        .arg(buttonBgColor.blue())
                                        .arg(theme->color(Plasma::Theme::HighlightColor).name(), "50%");
    buttonBgColor = theme->color(Plasma::Theme::TextColor);
    buttonStyleSheet += QString("QToolButton:hover { border: 2px solid %1; }")
                            .arg(theme->color(Plasma::Theme::HighlightColor).name());
    buttonStyleSheet += QString("QToolButton:focus { border: 2px solid %1; }")
                            .arg(theme->color(Plasma::Theme::HighlightColor).name());
    m_configButton->setStyleSheet(buttonStyleSheet);
    m_closeButton->setStyleSheet(buttonStyleSheet);

    connect(m_runnerManager, SIGNAL(matchesChanged(const QList<Plasma::QueryMatch>&)),
             this, SLOT(setMatches(const QList<Plasma::QueryMatch>&)));
    connect(m_matchView, SIGNAL(textChanged(const QString&)), this, SLOT(launchQuery(const QString&)));
    connect(m_matchView, SIGNAL(selectionChanged(MatchItem*)), this, SLOT(loadActions(MatchItem*)));
    connect(m_matchView, SIGNAL(itemActivated(MatchItem*)), this, SLOT(run(MatchItem*)));
    connect(m_actionView, SIGNAL(selectionChanged(MatchItem*)), this, SLOT(setAction(MatchItem*)));
    connect(m_actionView, SIGNAL(itemActivated(MatchItem*)), this, SLOT(run(MatchItem*)));

    m_matchView->setFocus();

    m_newQuery = true;
}

QsDialog::~QsDialog()
{}

// FIXME: We still have no notion of history... Actually adaptive search should partly take care of this
void QsDialog::clearHistory()
{}

void QsDialog::display(const QString &term)
{
    KWindowSystem::setOnDesktop(winId(), KWindowSystem::currentDesktop());

    m_matchView->reset();
    m_actionView->reset();
    m_actionView->hide();
    adjustSize();
    show();
    m_matchView->setFocus();

    int screen = 0;
    if (QApplication::desktop()->numScreens() > 1) {
        screen = QApplication::desktop()->screenNumber(QCursor::pos());
    }

    //KDialog::centerOnScreen(this, screen); // For some reason, this isn't working
    QDesktopWidget *desktop = QApplication::desktop();
    QRect r = desktop->screenGeometry(screen);
    move(r.left() + (r.width() / 2) - (width() / 2),
         r.top() + (r.height() / 2) - (height() / 2));

    KWindowSystem::forceActiveWindow(winId());
    if (term.isEmpty()) {
        m_matchView->setTitle(QString());
    } else {
        m_matchView->setTitle(term);
        launchQuery(term);
    }
}

void QsDialog::launchQuery(const QString &query)
{
    if (query.isEmpty()) {
        m_matchView->reset();
    } else {
        m_matchView->showLoading();
    }
    m_runnerManager->launchQuery(query);
    m_newQuery = true;
}

void QsDialog::run(MatchItem *item)
{
    if (QuickSand::QueryMatchItem *match = qobject_cast<QuickSand::QueryMatchItem*>(item)) {
        m_runnerManager->run(match->match());
        close();
    } else if (qobject_cast<QuickSand::QueryActionItem*>(item)) {
        m_runnerManager->run(m_currentMatch->match());
        close();
    }
}

void QsDialog::loadActions(MatchItem *item)
{
    if (item == m_currentMatch) {
        return;
    }
    m_currentMatch = qobject_cast<QuickSand::QueryMatchItem*>(item);
    QList<MatchItem*> actions;
    if (m_currentMatch) {
        QList<QAction*> queryActions = m_runnerManager->actionsForMatch(m_currentMatch->match());
        foreach(QAction *action, queryActions) {
            MatchItem *m = new QuickSand::QueryActionItem(action);
            actions.append(m);
        }
    }
    if (actions.size()) {
        m_actionView->show();
    } else if (m_actionView->isVisible()) {
        m_actionView->hide();
    }
    adjustSize();
    m_actionView->setItems(actions, false);
}

void QsDialog::setMatches(const QList<Plasma::QueryMatch> &matches)
{
    QList<MatchItem*> items;
    QMultiMap<QString, Plasma::QueryMatch> temp;
    QMultiMap<QString, Plasma::QueryMatch>::iterator end = m_matches.end();
    foreach (const Plasma::QueryMatch &match, matches) {
        temp.insert(match.id(), match);
        // Do not create new MatchItems for existing matches when the query hasn't changed
        if (!m_newQuery && m_matches.find(match.id()) != end) {
            // kDebug() << "A match with id " << match.id() << " already exists." << endl;
            QList<Plasma::QueryMatch> duplicates = m_matches.values(match.id());
            bool exists = false;
            foreach (const Plasma::QueryMatch &m, duplicates) {
                // FIXME: Matching the displayed text isn't always reliable
                // maybe adding an operator== to QueryMatch would help
                if (m.text() == match.text()) {
                    exists = true;
                    break;
                }
            }

            if (exists) {
                continue;
            }
        }
        MatchItem *m = new QuickSand::QueryMatchItem(match);
        switch(match.type())
        {
        case Plasma::QueryMatch::ExactMatch:
            m->setBackgroundColor(QColor(Qt::yellow));
            break;
        case Plasma::QueryMatch::InformationalMatch:
        case Plasma::QueryMatch::HelperMatch:
            m->setBackgroundColor(QColor(Qt::blue));
            break;
        default:
            m->setBackgroundColor(QColor(Qt::white));
            break;
        }
        items.append(m);
    }
    // kDebug() << "Add " << items.size() << " matches. Append?" << !m_newQuery << endl;
    m_matchView->setItems(items, true, !m_newQuery);
    m_matches = temp;
    // If new matches are obtained for the same query, append them to the list
    m_newQuery = false;
}

void QsDialog::setAction(MatchItem *item)
{
    if (QuickSand::QueryActionItem *action = qobject_cast<QuickSand::QueryActionItem*>(item)) {
        m_currentMatch->match().setSelectedAction(action->action());
    }
}

#include "qs_dialog.moc"
