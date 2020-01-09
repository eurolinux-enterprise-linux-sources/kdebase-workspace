/***************************************************************************
 *   taskarea.h                                                            *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
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

#ifndef TASKAREA_H
#define TASKAREA_H

#include <QGraphicsWidget>

#include "../protocols/dbussystemtray/dbussystemtraytask.h"

namespace SystemTray
{

class Applet;
class Task;

class TaskArea : public QGraphicsWidget
{
    Q_OBJECT

public:
    TaskArea(SystemTray::Applet *parent);
    ~TaskArea();

    void setHiddenTypes(const QStringList &hiddenTypes);
    bool isHiddenType(const QString &typeId, bool always = true) const;
    void setShowFdoTasks(bool show);
    bool showFdoTasks() const;
    void syncTasks(const QList<SystemTray::Task*> &tasks);
    bool hasHiddenTasks() const;
    int leftEasement() const;
    int rightEasement() const;
    void setOrientation(Qt::Orientation);

public slots:
    void addTask(SystemTray::Task *task);
    void removeTask(SystemTray::Task *task);

signals:
    void sizeHintChanged(Qt::SizeHint which);

private slots:
    void toggleHiddenItems();

private:
    void addWidgetForTask(SystemTray::Task *task);
    QGraphicsWidget* findWidget(Task *task);
    void updateUnhideToolIcon();
    void initUnhideTool();
    void checkUnhideTool();

    class Private;
    Private* const d;
};

}


#endif
