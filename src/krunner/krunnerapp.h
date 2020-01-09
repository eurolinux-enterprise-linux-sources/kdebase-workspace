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

#ifndef KRUNNERAPP_H
#define KRUNNERAPP_H

#include <kworkspace.h>

#include <kuniqueapplication.h>
#ifdef Q_WS_X11
#include "saverengine.h"
#endif

class KActionCollection;
class KDialog;

namespace Plasma
{
    class RunnerManager;
}

class KRunnerDialog;
class StartupId;

class KRunnerApp : public KUniqueApplication
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner.App")

public:
    static KRunnerApp* self();
    ~KRunnerApp();

    void logout( KWorkSpace::ShutdownConfirm confirm, KWorkSpace::ShutdownType sdtype );
    // The action collection of the active widget
    KActionCollection* actionCollection();

    virtual int newInstance();
#ifdef Q_WS_X11
    SaverEngine& screensaver() { return m_saver; }
#endif

    bool hasCompositeManager() const;

public Q_SLOTS:
    void logout();
    void logoutWithoutConfirmation();
    void haltWithoutConfirmation();
    void rebootWithoutConfirmation();

    // DBUS interface. if you change these methods, you MUST run:
    // qdbuscpp2xml -m krunnerapp.h -o org.kde.krunner.App.xml
    Q_SCRIPTABLE void initializeStartupNotification();

    /** Show taskmanager */
    Q_SCRIPTABLE void showTaskManager();
    /** Show taskmanager, filtering by the given string */
    Q_SCRIPTABLE void showTaskManagerWithFilter(const QString &filterText);

    /** Display the interface */
    Q_SCRIPTABLE void display();

    /** Display the interface */
    Q_SCRIPTABLE void query(const QString& term);

    /** Display the interface, using clipboard contents */
    Q_SCRIPTABLE void displayWithClipboardContents();

    /** Switch user */
    Q_SCRIPTABLE void switchUser();

    /** Clear the search history */
    Q_SCRIPTABLE void clearHistory();

private slots:
    /**
     * Called when the task dialog emits its finished() signal
     */
    void taskDialogFinished();
    void reloadConfig();
    void cleanUp();

private:
    KRunnerApp();
    void initialize();

    Plasma::RunnerManager *m_runnerManager;
    KActionCollection *m_actionCollection;
#ifdef Q_WS_X11
    SaverEngine m_saver;
#endif
    KRunnerDialog *m_interface;
    KDialog *m_tasks;
    StartupId *m_startupId;
};

#endif /* KRUNNERAPP_H */

