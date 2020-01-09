/***************************************************************************
 *   Copyright 2007 by Enrico Ros <enrico.ros@gmail.com>                   *
 *   Copyright 2007 by Riccardo Iaconelli <ruphy@kde.org>                  *
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

#ifndef __ResultItem_h__
#define __ResultItem_h__

#include <QtGui/QGraphicsWidget>
#include <QtGui/QIcon>

#include <Plasma/QueryMatch>

class QGraphicsLinearLayout;
class QGraphicsProxyWidget;

namespace Plasma
{
    class ToolButton;
    class RunnerManager;
} // namespace Plasma

class ResultItemSignaller : public QObject
{
    Q_OBJECT

public:
    ResultItemSignaller(QObject *parent = 0)
        : QObject(parent)
    {

    }

    void startAnimations()
    {
        emit animate();
    }

Q_SIGNALS:
    void animate();
};

class ResultItem : public QGraphicsWidget
{
    Q_OBJECT

public:
    ResultItem(const Plasma::QueryMatch &match, QGraphicsWidget *parent);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    void setMatch(const Plasma::QueryMatch &match);

    // getters
    QString id() const;
    QString name() const;
    QString description() const;
    QString data() const;
    QIcon icon() const;
    Plasma::QueryMatch::Type group() const;
    qreal priority() const;
    bool isFavorite() const;
    void setIndex(int index);
    int index() const;
    void setRowStride(int stride);
    void run(Plasma::RunnerManager *manager);
    bool isQueryPrototype() const;
    bool mouseHovered() const;
    void calculateSize();
    void calculateSize(int sceneWidth, int sceneHeight);

    static bool compare(const ResultItem *one, const ResultItem *other);
    bool operator<(const ResultItem &other) const;

    static const int TEXT_MARGIN = 3;
    static const int TIMER_INTERVAL = 40;

signals:
    void indexReleased(int index);
    void activated(ResultItem *item);
    void hoverEnter(ResultItem *item);
    void hoverLeave(ResultItem *item);
    void sizeChanged(ResultItem *item);

protected:
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *e);
    void timerEvent(QTimerEvent *e);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void keyPressEvent(QKeyEvent *event);
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);
    void changeEvent(QEvent *event);
    void resizeEvent(QGraphicsSceneResizeEvent *);
    void drawIcon(QPainter *painter, const QRect &iRect, const QPixmap &p);

protected slots:
    void showConfig();

private:
    // must always call remove()
    ~ResultItem();

private:
    Plasma::QueryMatch m_match;
    Plasma::ToolButton *m_configButton;

    QIcon m_icon;
    QBrush m_bgBrush;
    QPixmap m_fadeout;
    int m_highlight;
    int m_index;
    int m_highlightTimerId;
    bool m_mouseHovered;
    QGraphicsProxyWidget *m_configWidget;

    static int s_fontHeight;
};

#endif
