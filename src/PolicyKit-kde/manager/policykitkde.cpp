/*  This file is part of the KDE project
    Copyright (C) 2007-2008 Gökçen Eraslan <gokcen@pardus.org.tr>
    Copyright (C) 2008 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
    Copyright (C) 2008 Dario Freddi <drf54321@gmail.com>
    Copyright (C) 2008 Daniel Nicoletti <dantti85-pk@yahoo.com.br>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#include "policykitkde.h"
#include "authenticationagentadaptor.h"

#include <KDebug>
#include <QString>
#include <KMessageBox>
#include <KWindowSystem>

#include "qdbusconnection.h"

#include <QSocketNotifier>

//policykit header
#include <polkit-dbus/polkit-dbus.h>

#include <Context>

#include "AuthDialog.h"
#include "processwatcher.h"

#define THIRTY_SECONDS 30000

using namespace PolkitQt;

KCONFIGGROUP_DECLARE_ENUM_QOBJECT(PolicyKitKDE,KeepPassword)

PolicyKitKDE *PolicyKitKDE::m_self;

//----------------------------------------------------------------------------

PolicyKitKDE::PolicyKitKDE()
        : KUniqueApplication()
        , inProgress(false)
{
    Q_ASSERT(!m_self);
    m_self = this;

    setQuitOnLastWindowClosed(true);
    kDebug() << "Constructing PolicyKitKDE singleton";

    (void) new AuthenticationAgentAdaptor(this);
    if (!QDBusConnection::sessionBus().registerService("org.freedesktop.PolicyKit.AuthenticationAgent")) {
        kError() << "another authentication agent already running";
        QTimer::singleShot(0, this, SLOT(quit()));
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject("/", this)) {
        kError() << "unable to register service interface to dbus";
        QTimer::singleShot(0, this, SLOT(quit()));
        return;
    }

    m_context = polkit_context_new();
    polkit_context_set_load_descriptions(m_context);
    // we must pass the created context with the
    // polkit_context_set_load_descriptions() before the
    // lib inits it.
    Context::instance(m_context);

    if (Context::instance()->hasError()) {
        QTimer::singleShot(0, this, SLOT(quit()));
        return;
    }

    m_killT = new QTimer(this);
    connect(m_killT, SIGNAL(timeout()), this, SLOT(quit()));
    m_killT->start(THIRTY_SECONDS);
    kDebug() << "Kill timer set to " << THIRTY_SECONDS << " milliseconds";
}

//----------------------------------------------------------------------------

PolicyKitKDE::~PolicyKitKDE()
{
    kDebug() << "Exit";
}

//----------------------------------------------------------------------------

bool PolicyKitKDE::ObtainAuthorization(const QString &actionId, uint wid, uint pid)
{
    kDebug() << "Start obtain authorization:" << actionId << wid << pid;

    if (inProgress) {
        sendErrorReply("org.freedesktop.DBus.GLib.UnmappedError.PolkitKdeManagerError.Code1",
                       i18n("Another client is already authenticating, please try again later."));
        kDebug() << "Another client is already authenticating, please try again later.";
        return false;
    }

    inProgress = true;
    m_gainedPrivilege = false;
    m_requiresAdmin = false;
    m_keepPassword = KeepPasswordNo;

    m_pkAction = polkit_action_new();
    if (m_pkAction == NULL) {
        kError() << "Could not create new polkit action.";
        return false;
    }
    if (!polkit_action_set_action_id(m_pkAction, actionId.toLatin1())) {
        kError() << "Could not set actionid.";
        return false;
    }

    DBusError dbuserror;
    dbus_error_init(&dbuserror);
    DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, &dbuserror);
    caller = polkit_caller_new_from_pid(bus, pid, &dbuserror);
    if (caller == NULL) {
        kError() << QString("Could not define caller from pid: %1")
        .arg(QDBusError((const DBusError *)&dbuserror).message());
        dbus_connection_unref(bus);
        // TODO this all leaks and is probably pretty paranoid
        return false;
    }
    dbus_connection_unref(bus);

    PolKitPolicyCache *cache = polkit_context_get_policy_cache(m_context);
    if (cache == NULL) {
        kWarning() << "Could not get policy cache.";
        return false;
    }

    kDebug() << "Getting policy cache entry for an action...";
    PolKitPolicyFileEntry *entry = polkit_policy_cache_get_entry(cache, m_pkAction);
    if (entry == NULL) {
        kWarning() << "Could not get policy entry for action.";
        return false;
    }

    if (polkit_policy_file_entry_get_action_message(entry) == NULL) {
        kWarning() << "No message markup for given action";
        return false;
    }

    dialog = new AuthDialog(entry, pid);
    connect(dialog, SIGNAL(okClicked()), SLOT(dialogAccepted()));
    connect(dialog, SIGNAL(cancelClicked()), SLOT(dialogCancelled()));
    connect(dialog, SIGNAL(adminUserSelected(QString)), SLOT(userSelected(QString)));
    if (wid != 0) {
        KWindowSystem::setMainWindow(dialog, wid);
    } else {
        updateUserTimestamp(); // make it get focus unconditionally :-/
    }

    parent_wid = wid;

    message().setDelayedReply(true);
    reply = message().createReply();

    // Needed to don't need to return imediately
    setDelayedReply(true);

    m_killT->stop();
    m_numTries = 0;
    tryAgain();
    return false;
}

void PolicyKitKDE::tryAgain()
{
    grant = polkit_grant_new();
    polkit_grant_set_functions(grant,
                               add_io_watch,
                               add_child_watch,
                               remove_watch,
                               conversation_type,
                               conversation_select_admin_user,
                               conversation_pam_prompt_echo_off,
                               conversation_pam_prompt_echo_on,
                               conversation_pam_error_msg,
                               conversation_pam_text_info,
                               conversation_override_grant_type,
                               conversation_done,
                               this);
    m_wasCancelled = false;
    m_wasBogus = false;
    m_newUserSelected = false;

    if (!polkit_grant_initiate_auth(grant, m_pkAction, caller)) {
        kWarning() << "Failed to initiate privilege grant.";
        // send the reply over D-Bus:
        reply << true;
        QDBusConnection::sessionBus().send(reply);
        return;
    }
}




void PolicyKitKDE::finishObtainPrivilege()
{
    if (m_newUserSelected) {
        kDebug() << "new user selected so restarting auth..";
        polkit_grant_unref(grant);
        grant = NULL;
        tryAgain();
        return;
    }

    m_numTries++;

    kDebug() << QString("gained_privilege=%1 was_cancelled=%2 was_bogus=%3.").arg(m_gainedPrivilege)
    .arg(m_wasCancelled).arg(m_wasBogus);

    if (!m_gainedPrivilege && !m_wasCancelled && !m_wasBogus && dialog) {
        // Indicate the error
        dialog->incorrectPassword();
        if (m_numTries < 3) {
            polkit_grant_unref(grant);
            grant = NULL;
            tryAgain();
            return;
        }
    }

    if (m_gainedPrivilege) {
        // add to blacklist if the user unchecked the "remember authorization" check box
        char *action_id;
        polkit_action_get_action_id(m_pkAction, &action_id);
        // here we store the action preference
        KConfig config;
        KConfigGroup actionsPreferences(&config, "ActionsPreferences");
        writeEntry(actionsPreferences, action_id, m_keepPassword);
    }

    // send the reply over D-Bus:
    reply << m_gainedPrivilege;
    QDBusConnection::sessionBus().send(reply);

    if (dialog) {
        dialog->deleteLater();
        dialog = 0;
    }

    m_adminUsers.clear();
    m_adminUserSelected.clear();

    if (grant != NULL) {
        polkit_grant_unref(grant);
    }
    if (m_pkAction != NULL) {
        polkit_action_unref(m_pkAction);
    }
    if (caller != NULL) {
        polkit_caller_unref(caller);
    }

    grant = NULL;

    inProgress = false;
    m_killT->start(THIRTY_SECONDS);
    kDebug() << "Finish obtain authorization:" << m_gainedPrivilege;
}

PolicyKitKDE::KeepPassword PolicyKitKDE::readDefaultKeepPassword(const QString &actionId, const KeepPassword defaultValue)
{
    KConfig config;
    KConfigGroup actionsPreferences(&config, "ActionsPreferences");
    return readEntry(actionsPreferences, actionId.toLatin1(), defaultValue);
}

void PolicyKitKDE::conversation_type(PolKitGrant *grant, PolKitResult type, void *user_data)
{
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << "conversation_type" << grant << type;

    self->m_requiresAdmin = false;
    self->m_keepPassword = KeepPasswordNo;
    switch (type) {
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_ONE_SHOT:
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH:
            self->m_requiresAdmin = true;
            break;
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_SESSION:
            self->m_requiresAdmin = true;
            self->m_keepPassword = KeepPasswordSession;
            break;
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_ALWAYS:
            self->m_requiresAdmin = true;
            self->m_keepPassword = KeepPasswordAlways;
            break;
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_ONE_SHOT:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH:
            break;
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_SESSION:
            self->m_keepPassword = KeepPasswordSession;
            break;
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_ALWAYS:
            self->m_keepPassword = KeepPasswordAlways;
            break;
        default:
            abort();
    }

    self->dialog->setOptions(self->m_keepPassword, self->m_requiresAdmin, self->m_adminUsers);
}

char* PolicyKitKDE::conversation_select_admin_user(PolKitGrant *polkit_grant, char **admin_users, void *user_data)
{
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << "conversation_select_admin_user" << polkit_grant;
    QString currentAdminUser;

    /* if we've already selected the admin user.. then reuse the same one (this
     * is mainly when the user entered the wrong password)
     */
    if (!(currentAdminUser = self->dialog->adminUserSelected()).isEmpty()) {
        return strdup(currentAdminUser.toLocal8Bit());
    }

    QStringList adminUsers;
    for (int i = 0; admin_users[i] != NULL; i++) {
        adminUsers << admin_users[i];
    }
    self->m_adminUsers = adminUsers;
    self->dialog->setOptions(self->m_keepPassword, self->m_requiresAdmin, self->m_adminUsers);

    // if we are running as one of the users in adminUsers then preselect that user...
    if (!(currentAdminUser = self->dialog->selectCurrentAdminUser()).isEmpty()) {
        kDebug() << "Preselecting ourselves as adminUser";
        return strdup(currentAdminUser.toLocal8Bit());
    }

    // Wait for the user to select an user
    self->dialog->show();
    QEventLoop loop;
    connect(self->dialog, SIGNAL(adminUserSelected(QString)), &loop, SLOT(quit()));
    connect(self->dialog, SIGNAL(cancelClicked()), &loop, SLOT(quit()));
    loop.exec();

    /* if admin_user_selected is the empty string.. it means the dialog was
     * cancelled (see dialog_response() above)
     */
    if ((currentAdminUser = self->dialog->adminUserSelected()).isEmpty()) {
        polkit_grant_cancel_auth(polkit_grant);
        self->m_wasCancelled = true;
        return NULL;
    } else {
        return strdup(currentAdminUser.toLocal8Bit());
    }
}

void PolicyKitKDE::dialogAccepted()
{
    m_keepPassword = dialog->keepPassword();
    kDebug() << "Password dialog confirmed.";
}

void PolicyKitKDE::dialogCancelled()
{
    m_wasCancelled = true;
    kDebug() << "Password dialog cancelled.";
}

char* PolicyKitKDE::conversation_pam_prompt(PolKitGrant *polkit_grant, const char *request, void *user_data, bool echoOn)
{
    Q_UNUSED(polkit_grant);
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << QString("request=%1, echo_on=%2").arg(request).arg(echoOn);
    self->dialog->setRequest(request, self->m_requiresAdmin);
    self->dialog->setOptions(self->m_keepPassword, self->m_requiresAdmin, self->m_adminUsers);
    self->dialog->setPasswordShowChars(echoOn);
    self->dialog->show();

    self->m_dialogEventLoop = new QEventLoop(self);
    connect(self->dialog, SIGNAL(okClicked()), self->m_dialogEventLoop, SLOT(quit()));
    connect(self->dialog, SIGNAL(cancelClicked()), self->m_dialogEventLoop, SLOT(quit()));
    self->m_dialogEventLoop->exec(); // TODO this really sucks, policykit API is blocking
    delete self->m_dialogEventLoop;
    self->m_dialogEventLoop = 0;

    if (self->m_wasCancelled) {
        polkit_grant_cancel_auth(self->grant);
        return NULL;
    }

    return strdup(self->dialog->password().toLocal8Bit());
}

char* PolicyKitKDE::conversation_pam_prompt_echo_off(PolKitGrant *polkit_grant, const char *request, void *user_data)
{
    return conversation_pam_prompt(polkit_grant, request, user_data, false);
}

char* PolicyKitKDE::conversation_pam_prompt_echo_on(PolKitGrant *polkit_grant, const char *request, void *user_data)
{
    return conversation_pam_prompt(polkit_grant, request, user_data, true);
}

void PolicyKitKDE::userSelected(QString adminUser)
{
    kDebug() << QString("adminUser=%1").arg(adminUser);

    if (m_adminUserSelected.isEmpty()) {
        // happens when we're invoked from conversation_select_admin_user()
        m_adminUserSelected = adminUser;
    } else {
        kDebug() << "Restart auth as new user...";
        m_adminUserSelected = adminUser;
        m_newUserSelected = true;
        polkit_grant_cancel_auth(grant);
        // Here it means quit out of dialog event loop
        m_dialogEventLoop->quit();
    }
}

void PolicyKitKDE::conversation_pam_error_msg(PolKitGrant *polkit_grant, const char *msg, void *user_data)
{
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << "conversation_pam_error_msg" << polkit_grant << msg;
    KMessageBox::errorWId(self->dialog->isVisible() ? self->dialog->winId() : self->parent_wid,
                          QString::fromLocal8Bit(msg));
}

void PolicyKitKDE::conversation_pam_text_info(PolKitGrant *grant, const char *msg, void *user_data)
{
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << "conversation_pam_text_info" << grant << msg;
    KMessageBox::informationWId(self->dialog->isVisible() ? self->dialog->winId() : self->parent_wid,
                                QString::fromLocal8Bit(msg));
}

PolKitResult PolicyKitKDE::conversation_override_grant_type(PolKitGrant *polkit_grant, PolKitResult type, void *user_data)
{
    Q_UNUSED(polkit_grant);
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    kDebug() << type;
    bool keep_session = false;
    bool keep_always = false;

    switch (type) {
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_ONE_SHOT:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_ONE_SHOT:
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH:
            break;
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_SESSION:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_SESSION:
            if (self->m_keepPassword == KeepPasswordSession)
                keep_session = true;
            break;
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_ALWAYS:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_ALWAYS:
            if (self->m_keepPassword == KeepPasswordAlways)
                keep_always = true;
            else if (self->m_keepPassword == KeepPasswordSession)
                keep_session = true;
            break;
        default:
            abort();
    }
    kDebug() << "Keep password, always:" << keep_always << ", session:" << keep_session;
    PolKitResult ret;
    switch (type) {
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_ONE_SHOT:
            ret = POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_ONE_SHOT;
            break;
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH:
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_SESSION:
        case POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_ALWAYS:
            if (keep_session)
                ret = POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_SESSION;
            else if (keep_always)
                ret = POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH_KEEP_ALWAYS;
            else
                ret = POLKIT_RESULT_ONLY_VIA_ADMIN_AUTH;
            break;
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_ONE_SHOT:
            ret = POLKIT_RESULT_ONLY_VIA_SELF_AUTH_ONE_SHOT;
            break;
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_SESSION:
        case POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_ALWAYS:
            if (keep_session)
                ret = POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_SESSION;
            else if (keep_always)
                ret = POLKIT_RESULT_ONLY_VIA_SELF_AUTH_KEEP_ALWAYS;
            else
                ret = POLKIT_RESULT_ONLY_VIA_SELF_AUTH;
            break;
        default:
            abort();
    }
    return ret;
}

void PolicyKitKDE::conversation_done(PolKitGrant *polkit_grant,
                                     polkit_bool_t gainedPrivilege,
                                     polkit_bool_t inputWasBogus,
                                     void *user_data)
{
    Q_UNUSED(polkit_grant);
    PolicyKitKDE *self = (PolicyKitKDE *) user_data;
    self->m_gainedPrivilege = gainedPrivilege;
    self->m_wasBogus = inputWasBogus;

    kDebug() << QString("gained=%1, bogus=%2").arg(gainedPrivilege).arg(inputWasBogus);

    if (!self->dialog && (self->m_wasBogus || self->m_wasCancelled)) {
        self->dialog->deleteLater();
        self->dialog = 0;
    }
    QTimer::singleShot(0, self, SLOT(finishObtainPrivilege()));
}

//----------------------------------------------------------------------------

void PolicyKitKDE::watchActivatedGrant(int fd)
{
    Q_ASSERT(m_watches.contains(fd));
    kDebug() << "watchActivated" << m_watches[fd]->socket();//TODO this is being called more than one time
    polkit_grant_io_func(grant, m_watches[fd]->socket());
}

//----------------------------------------------------------------------------

int PolicyKitKDE::add_io_watch(PolKitGrant *polkit_grant, int fd)
{
    Q_UNUSED(polkit_grant);
    kDebug() << "add_watch" << fd;

    if (m_self->m_watches.contains(fd))
        return m_self->m_watches[fd]->socket();

    QSocketNotifier *notify = new QSocketNotifier(fd, QSocketNotifier::Read, m_self);
    m_self->m_watches[fd] = notify;

    notify->connect(notify, SIGNAL(activated(int)), m_self, SLOT(watchActivatedGrant(int)));

    return notify->socket(); // use simply the fd as the unique id for the watch
    // TODO this will be insufficient if there will be more watches for the same fd
}


//----------------------------------------------------------------------------

void PolicyKitKDE::remove_grant_io_watch(PolKitGrant *polkit_grant, int id)
{
    Q_UNUSED(polkit_grant);
    kDebug() << "remove_watch" << id;
    if (!m_self->m_watches.contains(id))
        return; // policykit likes to do this more than once

    QSocketNotifier* notify = m_self->m_watches.take(id);
    notify->deleteLater();
    notify->setEnabled(false);
}

//----------------------------------------------------------------------------

void PolicyKitKDE::watchActivatedContext(int fd)
{
    Q_ASSERT(m_watches.contains(fd));

//    kDebug() << "watchActivated" << fd;

    polkit_context_io_func(m_context, fd);
}

//----------------------------------------------------------------------------

int PolicyKitKDE::add_child_watch(PolKitGrant*, pid_t pid)
{
    ProcessWatch *watch = new ProcessWatch(pid);
    connect(watch, SIGNAL(terminated(pid_t, int)), m_self, SLOT(childTerminated(pid_t, int)));
    // return negative so that remove_watch() can tell io and child watches apart
    return - ProcessWatcher::instance()->add(watch);
}

//----------------------------------------------------------------------------

void PolicyKitKDE::remove_child_watch(PolKitGrant*, int id)
{
    Q_ASSERT(id < 0);
    ProcessWatcher::instance()->remove(-id);
}

//----------------------------------------------------------------------------

void PolicyKitKDE::childTerminated(pid_t pid, int exitStatus)
{
    polkit_grant_child_func(grant, pid, exitStatus);
}

//----------------------------------------------------------------------------

void PolicyKitKDE::remove_watch(PolKitGrant* grant, int id)
{
    // io watches are +, child watches are -
    if (id > 0) {
        remove_grant_io_watch(grant, id);
    } else {
        remove_child_watch(grant, id);
    }
}
