//===========================================================================
//
// This file is part of the KDE project
//
// Copyright 1999 Martin R. Jones <mjones@kde.org>
//


#include "saverengine.h"
#include "kscreensaversettings.h"
#include "screensaveradaptor.h"
#include "kscreensaveradaptor.h"

#include <kstandarddirs.h>
#include <kapplication.h>
#include <kservicegroup.h>
#include <krandom.h>
#include <kdebug.h>
#include <klocale.h>
#include <QFile>
#include <QX11Info>
#include <QDBusConnection>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include "xautolock_c.h"
extern xautolock_corner_t xautolock_corners[ 4 ];

//===========================================================================
//
// Screen saver engine. Doesn't handle the actual screensaver window,
// starting screensaver hacks, or password entry. That's done by
// a newly started process.
//
SaverEngine::SaverEngine()
    : QWidget()
{
    (void) new ScreenSaverAdaptor( this );
    QDBusConnection::sessionBus().registerService( "org.freedesktop.ScreenSaver" ) ;
    (void) new KScreenSaverAdaptor( this );
    QDBusConnection::sessionBus().registerService( "org.kde.screensaver" ) ;
    QDBusConnection::sessionBus().registerObject( "/ScreenSaver", this );

    // Save X screensaver parameters
    XGetScreenSaver(QX11Info::display(), &mXTimeout, &mXInterval,
                    &mXBlanking, &mXExposures);
    // And disable it. The internal X screensaver is not used at all, but we use its
    // internal idle timer (and it is also used by DPMS support in X). This timer must not
    // be altered by this code, since e.g. resetting the counter after activating our
    // screensaver would prevent DPMS from activating. We use the timer merely to detect
    // user activity.
    XSetScreenSaver(QX11Info::display(), 0, mXInterval, mXBlanking, mXExposures);

    mState = Waiting;
    mXAutoLock = 0;

    m_nr_throttled = 0;
    m_nr_inhibited = 0;
    m_actived_time = -1;

    connect(&mLockProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                        SLOT(lockProcessExited()));

    connect(QDBusConnection::sessionBus().interface(),
                SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            SLOT(serviceOwnerChanged(QString,QString,QString)));

    // Also receive updates triggered through the DBus (from powerdevil) see Bug #177123
    QStringList modules;
    QDBusInterface kdedInterface("org.kde.kded", "/kded", "org.kde.kded");
    QDBusReply<QStringList> reply = kdedInterface.call("loadedModules");

    if (!reply.isValid()) {
        return;
    }

    modules = reply.value();

    if (modules.contains("powerdevil")) {
      if (!QDBusConnection::sessionBus().connect("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil",
                          "DPMSconfigUpdated", this, SLOT(configure()))) {
            kDebug() << "error!";
        }
    }

    // I make it a really random number to avoid
    // some assumptions in clients, but just increase
    // while gnome-ss creates a random number every time
    m_next_cookie = KRandom::random() % 20000;
    configure();
}

//---------------------------------------------------------------------------
//
// Destructor - usual cleanups.
//
SaverEngine::~SaverEngine()
{
    delete mXAutoLock;

    // Restore X screensaver parameters
    XSetScreenSaver(QX11Info::display(), mXTimeout, mXInterval, mXBlanking,
                    mXExposures);
}

//---------------------------------------------------------------------------

void SaverEngine::Lock()
{
    if (mState == Waiting)
    {
        startLockProcess( ForceLock );
    }
    else
    {
        // XXX race condition here
        ::kill(mLockProcess.pid(), SIGHUP);
    }
}

void SaverEngine::processLockTransactions()
{
    QList<QDBusMessage>::ConstIterator it = mLockTransactions.constBegin(),
                                      end = mLockTransactions.constEnd();
    for ( ; it != end; ++it )
    {
        QDBusConnection::sessionBus().send(*it);
    }
    mLockTransactions.clear();
}

void SaverEngine::saverLockReady()
{
    if( mState != Preparing )
    {
        kDebug() << "Got unexpected saverLockReady()";
        return;
    }
    kDebug() << "Saver Lock Ready";
    processLockTransactions();
    if (m_nr_throttled)
        ::kill(mLockProcess.pid(), SIGSTOP);
}

void SaverEngine::SimulateUserActivity()
{
    XForceScreenSaver( QX11Info::display(), ScreenSaverReset );
    if ( mXAutoLock && mState == Waiting )
    {
        mXAutoLock->resetTrigger();
    }
}

//---------------------------------------------------------------------------
bool SaverEngine::save()
{
    if (mState == Waiting)
    {
        return startLockProcess( DefaultLock );
    }
    return false;
}

bool SaverEngine::setupPlasma()
{
    if (mState == Waiting)
    {
        return startLockProcess( PlasmaSetup );
    }
    return false;
}

//---------------------------------------------------------------------------
bool SaverEngine::quit()
{
    if (mState == Saving || mState == Preparing)
    {
        stopLockProcess();
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
bool SaverEngine::isEnabled()
{
    return mXAutoLock != 0;
}

//---------------------------------------------------------------------------
bool SaverEngine::enable( bool e, bool force )
{
    if ( !force && e == isEnabled() )
        return true;

    // If we aren't in a suitable state, we will not reconfigure.
    if (mState != Waiting)
        return false;

    if (e)
    {
        if (!mXAutoLock)
        {
            mXAutoLock = new XAutoLock();
            connect(mXAutoLock, SIGNAL(timeout()), SLOT(idleTimeout()));
        }

        int timeout = KScreenSaverSettings::timeout();
        mXAutoLock->setTimeout(timeout);
        mXAutoLock->setDPMS(true);
#ifdef NOT_FREAKIN_UGLY
        mXAutoLock->changeCornerLockStatus( mLockCornerTopLeft, mLockCornerTopRight, mLockCornerBottomLeft, mLockCornerBottomRight);
#else
        xautolock_corners[0] = applyManualSettings(KScreenSaverSettings::actionTopLeft());
        xautolock_corners[1] = applyManualSettings(KScreenSaverSettings::actionTopRight());
        xautolock_corners[2] = applyManualSettings(KScreenSaverSettings::actionBottomLeft());
        xautolock_corners[3] = applyManualSettings(KScreenSaverSettings::actionBottomRight());
#endif

        mXAutoLock->start();
        kDebug() << "Saver Engine started, timeout: " << timeout;
    }
    else
    {
        delete mXAutoLock;
        mXAutoLock = 0;
        kDebug() << "Saver Engine disabled";
    }

    return true;
}

//---------------------------------------------------------------------------
bool SaverEngine::isBlanked()
{
  return (mState != Waiting);
}

//---------------------------------------------------------------------------
//
// Read and apply configuration.
//
void SaverEngine::configure()
{
    // create a new config obj to ensure we read the latest options
    KScreenSaverSettings::self()->readConfig();

    enable( KScreenSaverSettings::screenSaverEnabled(), true );
}

//---------------------------------------------------------------------------
//
// Start the screen saver.
//
bool SaverEngine::startLockProcess( LockType lock_type )
{
    Q_ASSERT(mState == Waiting);

    kDebug() << "SaverEngine: starting saver";

    QString path = KStandardDirs::findExe( "kscreenlocker" );
    if( path.isEmpty())
    {
        kDebug() << "Can't find kscreenlocker!";
        return false;
    }
    mLockProcess.clearProgram();
    mLockProcess << path;
    switch( lock_type )
    {
    case ForceLock:
        mLockProcess << QString( "--forcelock" );
        break;
    case DontLock:
        mLockProcess << QString( "--dontlock" );
        break;
    case PlasmaSetup:
        mLockProcess << "--plasmasetup";
        break;
    default:
        break;
    }

    m_actived_time = time( 0 );
    mLockProcess.start();
    if (mLockProcess.waitForStarted() == false )
    {
        kDebug() << "Failed to start kscreenlocker!";
        m_actived_time = -1;
        return false;
    }

    if (mXAutoLock)
    {
        mXAutoLock->stop();
    }

    emit ActiveChanged(true); // DBus signal
    mState = Preparing;

    // It takes a while for kscreenlocker to start and lock the screen.
    // Therefore delay the DBus call until it tells krunner that the locking is in effect.
    // This is done only for --forcelock .
    if (lock_type == ForceLock && calledFromDBus()) {
        mLockTransactions.append(message().createReply());
        setDelayedReply(true);
    }

    return true;
}

//---------------------------------------------------------------------------
//
// Stop the screen saver.
//
void SaverEngine::stopLockProcess()
{
    Q_ASSERT(mState != Waiting);
    kDebug() << "SaverEngine: stopping lock process";

    mLockProcess.kill();
}

void SaverEngine::lockProcessExited()
{
    Q_ASSERT(mState != Waiting);
    kDebug() << "SaverEngine: lock process exited";

    if (mXAutoLock)
    {
        mXAutoLock->start();
    }

    processLockTransactions();
    emit ActiveChanged(false); // DBus signal
    m_actived_time = -1;
    mState = Waiting;
}

//---------------------------------------------------------------------------
//
// XAutoLock has detected the required idle time.
//
void SaverEngine::idleTimeout()
{
    if( mState != Waiting )
        return; // already saving
    startLockProcess( DefaultLock );
}

xautolock_corner_t SaverEngine::applyManualSettings(int action)
{
    if (action == 0)
    {
        kDebug() << "no lock";
        return ca_nothing;
    }
    else if (action == 1)
    {
        kDebug() << "lock screen";
        return ca_forceLock;
    }
    else if (action == 2)
    {
        kDebug() << "prevent lock";
        return ca_dontLock;
    }
    else
    {
        kDebug() << "no lock nothing";
        return ca_nothing;
    }
}

uint SaverEngine::GetSessionIdleTime()
{
    return mXAutoLock ? mXAutoLock->idleTime() : 0;
}

uint SaverEngine::GetActiveTime()
{
    if ( m_actived_time == -1 )
        return 0;
    return time( 0 ) - m_actived_time;
}

bool SaverEngine::GetActive()
{
    return ( mState != Waiting );
}

bool SaverEngine::SetActive(bool state)
{
    if ( state )
        return save();
    else
        return quit();
}

uint SaverEngine::Inhibit(const QString &/*application_name*/, const QString &/*reason*/)
{
    ScreenSaverRequest sr;
//     sr.appname = application_name;
//     sr.reasongiven = reason;
    sr.cookie = m_next_cookie++;
    sr.dbusid = message().service();
    sr.type = ScreenSaverRequest::Inhibit;
    m_requests.append( sr );
    m_nr_inhibited++;
    enable( false );
    return sr.cookie;
}

void SaverEngine::UnInhibit(uint cookie)
{
    QMutableListIterator<ScreenSaverRequest> it( m_requests );
    while ( it.hasNext() )
    {
        if ( it.next().cookie == cookie ) {
            it.remove();
            if ( !--m_nr_inhibited )
                enable( true );
        }
    }
}

uint SaverEngine::Throttle(const QString &/*application_name*/, const QString &/*reason*/)
{
    ScreenSaverRequest sr;
//     sr.appname = application_name;
//     sr.reasongiven = reason;
    sr.cookie = m_next_cookie++;
    sr.type = ScreenSaverRequest::Throttle;
    sr.dbusid = message().service();
    m_requests.append( sr );
    m_nr_throttled++;
    if (mLockProcess.state() == QProcess::Running)
        ::kill(mLockProcess.pid(), SIGSTOP);
    return sr.cookie;
}

void SaverEngine::UnThrottle(uint cookie)
{
    QMutableListIterator<ScreenSaverRequest> it( m_requests );
    while ( it.hasNext() )
    {
        if ( it.next().cookie == cookie ) {
            it.remove();
            if ( !--m_nr_throttled )
                if (mLockProcess.state() == QProcess::Running)
                    ::kill(mLockProcess.pid(), SIGCONT);
        }
    }
}

void SaverEngine::serviceOwnerChanged(const QString& name, const QString &, const QString &newOwner)
{
    if ( !newOwner.isEmpty() ) // looking for deaths
        return;

    QListIterator<ScreenSaverRequest> it( m_requests );
    while ( it.hasNext() )
    {
        const ScreenSaverRequest &r = it.next();
        if ( r.dbusid == name )
        {
            if ( r.type == ScreenSaverRequest::Throttle )
                UnThrottle( r.cookie );
            else
                UnInhibit( r.cookie );
        }
    }
}

#include "saverengine.moc"


