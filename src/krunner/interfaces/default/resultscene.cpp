/***************************************************************************
*   Copyright 2007 by Enrico Ros <enrico.ros@gmail.com>                   *
*   Copyright 2007 by Riccardo Iaconelli <ruphy@kde.org>                  *
*   Copyright 2008 by Aaron Seigo <aseigo@kde.org>                        *
*   Copyright 2008 by Davide Bettio <davide.bettio@kdemail.net>           *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
***************************************************************************/

#include "resultscene.h"

#include <QtCore/QDebug>
#include <QtGui/QKeyEvent>
#include <QtCore/QMutexLocker>
#include <QtGui/QPainter>
#include <QtCore/QTimeLine>
#include <QtGui/QGraphicsSceneWheelEvent>
#include <QtGui/QGraphicsGridLayout>
#include <QtGui/QGraphicsWidget>
#include <QtGui/QGraphicsProxyWidget>

#include <KDE/KDebug>
#include <KDE/KIconLoader>
#include <KDE/KLineEdit>

#include <Plasma/AbstractRunner>
#include <Plasma/RunnerManager>

#include "resultitem.h"
#include "selectionbar.h"

ResultScene::ResultScene(Plasma::RunnerManager *manager, QWidget *focusBase, QObject *parent)
    : QGraphicsScene(parent),
      m_runnerManager(manager),
      m_currentIndex(0),
      m_focusBase(focusBase)
{
    setItemIndexMethod(NoIndex);

    connect(m_runnerManager, SIGNAL(matchesChanged(const QList<Plasma::QueryMatch>&)),
            this, SLOT(setQueryMatches(const QList<Plasma::QueryMatch>&)));

    m_clearTimer.setSingleShot(true);
    connect(&m_clearTimer, SIGNAL(timeout()), this, SLOT(clearMatches()));

    m_hoverTimer.setSingleShot(true);
    connect(&m_hoverTimer, SIGNAL(timeout()), this, SLOT(initItemsHoverEvents()));

    m_selectionBar = new SelectionBar(0);
    addItem(m_selectionBar);

    connect(m_selectionBar, SIGNAL(ensureVisibility(QGraphicsItem *)), this, SIGNAL(ensureVisibility(QGraphicsItem *)));

    m_selectionBar->hide();
    updateItemMargins();

    connect(m_selectionBar, SIGNAL(graphicsChanged()), this, SLOT(updateItemMargins()));
    //QColor bg(255, 255, 255, 126);
    //setBackgroundBrush(bg);
}

ResultScene::~ResultScene()
{
    clearMatches();
    delete m_selectionBar;
}

QSize ResultScene::minimumSizeHint() const
{
    QFontMetrics fm(font());
    return QSize(KIconLoader::SizeMedium * 4, (fm.height() * 5) * 3);
}

void ResultScene::resize(int width, int height)
{
    bool resizeItems = width != sceneRect().width();

    if (resizeItems) {
        foreach (ResultItem *item, m_items) {
            item->calculateSize(width, height);
        }
    }
    setSceneRect(itemsBoundingRect());
}

void ResultScene::clearMatches()
{
    foreach (ResultItem *item, m_items) {
        removeItem(item);
        item->deleteLater();
    }

    m_itemsById.clear();
    m_items.clear();
    emit matchCountChanged(0);
}

bool ResultScene::canMoveItemFocus() const
{
    // We prevent a late query result from stealing the item focus from the user
    // The item focus can be moved only if
    // 1) there is no item currently focused
    // 2) the currently focused item is not visible anymore
    // 3) the focusBase widget (the khistorycombobox) has focus (i.e. the user is still typing or waiting) AND the currently focused item has not been hovered

    return !(focusItem()) || (!m_items.contains(static_cast<ResultItem*>(focusItem()))) || (m_focusBase->hasFocus() && !static_cast<ResultItem*>(focusItem())->mouseHovered()) ;

}

void ResultScene::setItemsAcceptHoverEvents(bool enable)
{
    foreach (QGraphicsItem* tmpItem, items()) {
        tmpItem->setAcceptHoverEvents(enable);
    }
}

void ResultScene::initItemsHoverEvents()
{
    setItemsAcceptHoverEvents(true);
}

void ResultScene::setQueryMatches(const QList<Plasma::QueryMatch> &m)
{
    //kDebug() << "============================" << endl << "matches retrieved: " << m.count();
    /*
    foreach (const Plasma::QueryMatch &match, m) {
        kDebug() << "    " << match.id() << match.text();
    }
    */

    if (m.isEmpty()) {
        //kDebug() << "clearing";
        resize(width(), 0);
        emit itemHoverEnter(0);
        m_clearTimer.start(200);
        return;
    }

    m_hoverTimer.stop();
    setItemsAcceptHoverEvents(false);

    //resize(width(), m.count() * ResultItem::BOUNDING_HEIGHT);
    m_clearTimer.stop();
    m_items.clear();

    QList<Plasma::QueryMatch> matches = m;
    QMutableListIterator<Plasma::QueryMatch> newMatchIt(matches);

    // first pass: we try and match up items with existing ids (match persisitence)
    while (!m_itemsById.isEmpty() && newMatchIt.hasNext()) {
        ResultItem *item = addQueryMatch(newMatchIt.next(), false);

        if (item) {
            m_items.append(item);
            newMatchIt.remove();
        }
    }

    // second pass: now we just use any item that exists (item re-use)
    newMatchIt.toFront();
    while (newMatchIt.hasNext()) {
        m_items.append(addQueryMatch(newMatchIt.next(), true));
    }

    // delete the stragglers
    QMapIterator<QString, ResultItem *> it(m_itemsById);
    while (it.hasNext()) {
        removeItem(it.next().value());
        it.value()->deleteLater();
    }

    // organize the remainders
    m_itemsById.clear();

    // this will leave them in *reverse* order
    qSort(m_items.begin(), m_items.end(), ResultItem::compare);

    emit matchCountChanged(m.count());
    arrangeItems(0);

    m_hoverTimer.start(200);

}

void ResultScene::arrangeItems(ResultItem *itemChanged)
{
    QListIterator<ResultItem*> matchIt(m_items);
    QGraphicsWidget *tab = 0;
    int y = 0;
    int i = 0;
    while (matchIt.hasNext()) {
        ResultItem *item = matchIt.next();
        //kDebug()  << item->name() << item->id() << item->priority() << i;

        if (!itemChanged) {
            // first time set up
            QGraphicsWidget::setTabOrder(tab, item);
            m_itemsById.insert(item->id(), item);
            item->setIndex(i);
        }

        item->setPos(0, y);
        item->show();
        //kDebug() << item->pos();
        y += item->geometry().height();

        // it is vital that focus is set *after* the index
        if (!itemChanged && i == 0 && canMoveItemFocus()) {
            clearSelection();
            setFocusItem(item);
            item->setSelected(true);
            emit ensureVisibility(item);
        }

        ++i;
        tab = item;
    }

    // Here we cannot use itemsBoundingRect().height() because old items will be deleted on the next event cycle
    // However we use itemsBoundingRect().width() to take care of width changes.
    setSceneRect(QRect(0,0,itemsBoundingRect().width(),y));
}

ResultItem* ResultScene::addQueryMatch(const Plasma::QueryMatch &match, bool useAnyId)
{
    QMap<QString, ResultItem*>::iterator it = useAnyId ? m_itemsById.begin() : m_itemsById.find(match.id());
    ResultItem *item = 0;
    //kDebug() << "attempting" << match.id();

    if (it == m_itemsById.end()) {
        //kDebug() << "did not find for" << match.id();
        if (useAnyId) {
            //kDebug() << "creating for" << match.id();
            item = new ResultItem(match, 0);
            item->setContentsMargins(m_itemMarginLeft, m_itemMarginTop,
                                     m_itemMarginRight, m_itemMarginBottom);
            addItem(item);
            item->hide();
            connect(item, SIGNAL(sizeChanged(ResultItem*)), this, SLOT(arrangeItems(ResultItem*)));
            connect(item, SIGNAL(activated(ResultItem*)), this, SIGNAL(itemActivated(ResultItem*)));
            connect(item, SIGNAL(hoverEnter(ResultItem*)), this, SIGNAL(itemHoverEnter(ResultItem*)));
            connect(item, SIGNAL(hoverLeave(ResultItem*)), this, SIGNAL(itemHoverLeave(ResultItem*)));
        } else {
            //kDebug() << "returning failure for" << match.id();
            return 0;
        }
    } else {
        item = it.value();
        //kDebug() << "reusing" << item->name() << "for" << match.id();
        item->setMatch(match);
        m_itemsById.erase(it);
    }

    return item;
}


void ResultScene::focusInEvent(QFocusEvent *focusEvent)
{

    // The default implementation of focusInEvent assumes that if the scene has no focus
    // then it has no focused item; thus, when a scene gains focus, focusInEvent gives
    // focus to the last focused item.
    // In our case this assumption is not true, as an item can be focused before the scene,
    // therefore we revert the behaviour by re-selecting the previously selected item

    ResultItem *currentFocus = dynamic_cast<ResultItem*>(focusItem());

    QGraphicsScene::focusInEvent(focusEvent);

    switch (focusEvent->reason()) {
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
        break;
    default:
        if (currentFocus) {
            setFocusItem(currentFocus);
        }
        break;
    }
}

void ResultScene::focusOutEvent(QFocusEvent *focusEvent)
{
    QGraphicsScene::focusOutEvent(focusEvent);
    if (!m_items.isEmpty()) {
        emit itemHoverEnter(m_items.at(0));
    }
}

void ResultScene::keyPressEvent(QKeyEvent * keyEvent)
{
    //kDebug() << "m_items (size): " << m_items.size() << "\n";
    switch (keyEvent->key()) {
        case Qt::Key_Up:
        case Qt::Key_Left:
            selectNextItem();
            break;

        case Qt::Key_Down:
        case Qt::Key_Right:
            selectPreviousItem();
        break;

        default:
            // pass the event to the item
            QGraphicsScene::keyPressEvent(keyEvent);
            return;
        break;
    }
}

void ResultScene::selectNextItem()
{
    ResultItem *currentFocus = dynamic_cast<ResultItem*>(focusItem());
    int m_currentIndex = currentFocus ? currentFocus->index() : 0;

    if (m_currentIndex > 0) {
        --m_currentIndex;
    } else {
        m_currentIndex = m_items.size() - 1;
    }

    setFocusItem(m_items.at(m_currentIndex));
    clearSelection();
    m_items.at(m_currentIndex)->setSelected(true);
}

void ResultScene::selectPreviousItem()
{
    ResultItem *currentFocus = dynamic_cast<ResultItem*>(focusItem());
    int m_currentIndex = currentFocus ? currentFocus->index() : 0;

    ++m_currentIndex;

    if (m_currentIndex >= m_items.size()) {
        m_currentIndex = 0;
    }

    setFocusItem(m_items.at(m_currentIndex));
    clearSelection();
    m_items.at(m_currentIndex)->setSelected(true);
}

bool ResultScene::launchQuery(const QString &term)
{
    bool temp = !(term.trimmed().isEmpty() || m_runnerManager->query() == term.trimmed());
    m_runnerManager->launchQuery(term);
    return temp;
}

bool ResultScene::launchQuery(const QString &term, const QString &runner)
{
    bool temp = !(term.trimmed().isEmpty() || m_runnerManager->query() == term.trimmed());
    m_runnerManager->launchQuery(term, runner);
    return temp;
}

void ResultScene::clearQuery()
{
    m_runnerManager->reset();
}

ResultItem* ResultScene::defaultResultItem() const
{
    if (m_items.isEmpty()) {
        kDebug() << "empty";
        return 0;
    }

    kDebug() << (QObject*) m_items[0] << m_items.count();
    return m_items[0];
}

void ResultScene::run(ResultItem *item) const
{
    if (!item) {
        return;
    }

    item->run(m_runnerManager);
}

/*
Plasma::RunnerManager* ResultScene::manager() const
{
    return m_runnerManager;
}
*/
void ResultScene::updateItemMargins()
{
    m_selectionBar->getMargins(m_itemMarginLeft, m_itemMarginTop,
                               m_itemMarginRight, m_itemMarginBottom);

    foreach (ResultItem *item, m_items) {
        item->setContentsMargins(m_itemMarginLeft, m_itemMarginTop,
                                m_itemMarginRight, m_itemMarginBottom);
    }
}

#include "resultscene.moc"

