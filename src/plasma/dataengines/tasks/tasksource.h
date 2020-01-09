/*
 * Copyright 2008 Alain Boyer <alainboyer@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef TASKSOURCE_H
#define TASKSOURCE_H

// plasma
#include <Plasma/DataContainer>

// libtaskmanager
#include <taskmanager/taskmanager.h>
using TaskManager::StartupPtr;
using TaskManager::TaskPtr;

/**
 * Task Source
 *
 * This custom DataContainer represents a task or startup task as a unique source.
 * It holds a shared pointer to the task or startup task and is responsible for setting
 * and updating the data exposed by the source.
 */
class TaskSource : public Plasma::DataContainer
{

    Q_OBJECT

    public:
        TaskSource(StartupPtr startup, QObject *parent);
        TaskSource(TaskPtr task, QObject *parent);
        ~TaskSource();

    protected:
        Plasma::Service *createService();
        TaskPtr getTask();
        bool isTask() const;

    private slots:
        void updateStartup(::TaskManager::TaskChanges startupChanges);
        void updateTask(::TaskManager::TaskChanges taskChanges);
        void updateDesktop(int desktop);

    private:
        friend class TasksEngine;
        friend class TaskJob;
        StartupPtr m_startup;
        TaskPtr m_task;
        bool m_isTask;

};

#endif // TASKSOURCE_H
