/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

//#define QT_CLEAN_NAMESPACE
#include "tabbox.h"
#include <QTextStream>
#include "workspace.h"
#include "effects.h"
#include "client.h"
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QTimeLine>
#include <kglobal.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <klocale.h>
#include <QAction>
#include <stdarg.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kiconeffect.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <QX11Info>
#include <kactioncollection.h>
#include <kkeyserver.h>
#include <kconfiggroup.h>
#include <KDE/Plasma/Theme>
#include <KGlobalSettings>

// specify externals before namespace

namespace KWin
{

extern QPixmap* kwin_get_menu_pix_hack();

TabBox::TabBox( Workspace *ws )
    : QGraphicsView()
    , wspace(ws)
    , client(0)
    , display_refcount( 0 )
    , selectionItem( 0 )
    {
    scene = new QGraphicsScene( this );
    setWindowFlags( Qt::X11BypassWindowManagerHint );
    setScene( scene );
    setFrameStyle( QFrame::NoFrame );
    viewport()->setAutoFillBackground( false );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setAttribute( Qt::WA_TranslucentBackground );
    frame.setImagePath( "dialogs/background" );
    frame.setCacheAllRenderedFrames( true );
    frame.setEnabledBorders( Plasma::FrameSvg::AllBorders );
    item_frame.setImagePath( "widgets/viewitem" );
    item_frame.setElementPrefix( "hover" );
    item_frame.setCacheAllRenderedFrames( true );
    item_frame.setEnabledBorders( Plasma::FrameSvg::AllBorders );

    showMiniIcon = false;

    no_tasks = i18n("*** No Windows ***");
    m = TabBoxDesktopMode; // init variables
    reconfigure();
    reset();
    connect(&delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
    XSetWindowAttributes attr;
    attr.override_redirect = 1;
    outline_left = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outline_right = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outline_top = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outline_bottom = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    }

TabBox::~TabBox()
    {
    XDestroyWindow( QX11Info::display(), outline_left );
    XDestroyWindow( QX11Info::display(), outline_right );
    XDestroyWindow( QX11Info::display(), outline_top );
    XDestroyWindow( QX11Info::display(), outline_bottom );
    }


/*!
  Sets the current mode to \a mode, either TabBoxDesktopListMode or TabBoxWindowsMode

  \sa mode()
 */
void TabBox::setMode( TabBoxMode mode )
    {
    m = mode;
    }


/*!
  Create list of clients on specified desktop, starting with client c
*/
void TabBox::createClientList(ClientList &list, int desktop /*-1 = all*/, Client *c, bool chain)
    {
    ClientList::size_type idx = 0;

    list.clear();

    Client* start = c;

    if ( chain )
        c = workspace()->nextClientFocusChain(c);
    else
        c = workspace()->stackingOrder().first();

    Client* stop = c;

    while ( c )
        {
        Client* add = NULL;
        if ( ((desktop == -1) || c->isOnDesktop(desktop))
             && c->wantsTabFocus() )
            { // don't add windows that have modal dialogs
            Client* modal = c->findModal();
            if( modal == NULL || modal == c )
                add = c;
            else if( !list.contains( modal ))
                add = modal;
            else
                {
                // nothing
                }
            }
        if( options->separateScreenFocus && options->xineramaEnabled )
            {
            if( c->screen() != workspace()->activeScreen())
                add = NULL;
            }
        if( add != NULL )
            {
            if ( start == add )
                {
                list.removeAll( add );
                list.prepend( add );
                }
            else
                list += add;
            }
        if ( chain )
          c = workspace()->nextClientFocusChain( c );
        else
          {
          if ( idx >= (workspace()->stackingOrder().size()-1) )
            c = 0;
          else
            c = workspace()->stackingOrder()[++idx];
          }

        if ( c == stop )
            break;
        }
    }


/*!
  Create list of desktops, starting with desktop start
*/
void TabBox::createDesktopList(QList< int > &list, int start, SortOrder order)
    {
    list.clear();

    int iDesktop = start;

    for( int i = 1; i <= workspace()->numberOfDesktops(); i++ )
        {
        list.append( iDesktop );
        if ( order == StaticOrder )
            {
            iDesktop = workspace()->nextDesktopStatic( iDesktop );
            }
        else
            { // MostRecentlyUsedOrder
            iDesktop = workspace()->nextDesktopFocusChain( iDesktop );
            }
        }
    }


/*!
  Resets the tab box to display the active client in TabBoxWindowsMode, or the
  current desktop in TabBoxDesktopListMode
 */
void TabBox::reset( bool partial_reset )
    {
    int w, h, cw = 0, wmax = 0;
    qreal left, top, right, bottom;
    frame.getMargins( left, top, right, bottom );

    QRect r = workspace()->screenGeometry( workspace()->activeScreen());

    // calculate height of 1 line
    // fontheight + 2 pixel above + 2 pixel below, or 32x32 icon + 4 pixel above + below
    lineHeight = top + bottom + qMax(QFontMetrics( KGlobalSettings::smallestReadableFont() ).height(), 32 );

    if ( mode() == TabBoxWindowsMode )
        {
        Client* starting_client = 0;
        if( partial_reset && clients.count() != 0 )
            starting_client = clients.first();
        else
            client = starting_client = workspace()->activeClient();

        // get all clients to show
        createClientList(clients, options_traverse_all ? -1 : workspace()->currentDesktop(), starting_client, true);

        // calculate maximum caption width
        cw = fontMetrics().width(no_tasks) + 20;
        QFont f = font();
        f.setPointSize( KGlobalSettings::smallestReadableFont().pointSize() );
        QFontMetrics fm = QFontMetrics( f );
        for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
          {
          cw = fm.width( (*it)->caption() );
          if( (*it)->desktop() != wspace->currentDesktop() )
            cw += fm.width( QString( wspace->desktopName((*it)->desktop()) + ": " ) );
          if ( cw > wmax ) wmax = cw;
          }

        // calculate height for the popup
        if ( clients.count() == 0 )  // height for the "not tasks" text
          {
          QFont f = font();
          f.setBold( true );
          f.setPointSize( 14 );

          h = QFontMetrics(f).height()*4;
          }
        else
          {
          showMiniIcon = false;
          h = clients.count() * lineHeight;

          if ( h > (r.height()-(top + bottom)) )  // if too high, use mini icons
            {
            showMiniIcon = true;
            // fontheight + 1 pixel above + 1 pixel below, or 16x16 icon + 2 pixel above + below
            lineHeight = qMax(QFontMetrics( KGlobalSettings::smallestReadableFont() ).height() + 2, 16 + 4);

            h = clients.count() * lineHeight;

            if ( h > (r.height()-(top + bottom)) ) // if still too high, remove some clients
              {
                // how many clients to remove
                int howMany = (h - (r.height()-(top + bottom)))/lineHeight + 1;
                for (; howMany; howMany--)
                  clients.removeAll(clients.last());

                h = clients.count() * lineHeight;
              }
            }
          }
        }
    else
        {
        int starting_desktop;
        if( mode() == TabBoxDesktopListMode )
            {
            starting_desktop = 1;
            createDesktopList(desktops, starting_desktop, StaticOrder );
            }
        else
            { // TabBoxDesktopMode
            starting_desktop = workspace()->currentDesktop();
            createDesktopList(desktops, starting_desktop, MostRecentlyUsedOrder );
            }

        if( !partial_reset )
            desk = workspace()->currentDesktop();

        showMiniIcon = false;

        foreach (int it, desktops)
          {
          cw = fontMetrics().width( workspace()->desktopName(it) );
          if ( cw > wmax ) wmax = cw;
          }

        // calculate height for the popup (max. 16 desktops always fit in a 800x600 screen)
        h = desktops.count() * lineHeight;
        }

    // height, width for the popup
    h += top + bottom;
    h = qMin( h, r.height() );
    w = left + right + 5*2 + ( showMiniIcon ? 16 : 32 ) + 8 + wmax; // 5*2=margins, ()=icon, 8=space between icon+text
    w = qBound( r.width()/5 , w, r.width() * 4 / 5 );

    setGeometry((r.width()-w)/2 + r.x(),
                 (r.height()-h)/2+ r.y(),
                 w, h );
    scene->setSceneRect( 0, 0, w, h );
    frame.resizeFrame( QSize( w , h ) );
    // resizing the item frame
    item_frame.resizeFrame( QSize( w-left-right, lineHeight ) );

    setMask( frame.mask() );

    if( partial_reset )
        initScene();

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }

void TabBox::initScene()
    {
    scene->clear();
    qreal left, top, right, bottom;
    frame.getMargins( left, top, right, bottom );

    if( KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects )
        {
        selectionItem = new TabBoxSelectionItem( this );
        selectionItem->setPos( left, top );
        selectionItem->hide();
        scene->addItem( selectionItem );
        }
    else
        {
        selectionItem = 0;
        }

    if( mode() == TabBoxWindowsMode )
        {
        if( clients.isEmpty() )
            {
            QFont f = font();
            f.setBold( true );
            f.setPointSize( 14 );

            QGraphicsTextItem* item = scene->addText( no_tasks, f );
            item->setDefaultTextColor( Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor ) );
            item->adjustSize();
            item->setPos( width()*0.5 - QFontMetrics(f).width( item->toPlainText() )*0.5,
                height()*0.5 - QFontMetrics(f).height()*0.5 );
            }
        else
            {
            // add clients to scene
            int index = 0;

            foreach( Client* c, clients )
                {
                TabBoxWindowItem* item = new TabBoxWindowItem( c, this );
                item->setHeight( lineHeight );
                item->setWidth( width() - left - right );
                item->setShowMiniIcons( showMiniIcon );
                item->setPos( left, top + lineHeight * index );
                scene->addItem( item );

                if ( selectionItem && c == client )
                    {
                    selectionItem->setPos( item->pos() );
                    }

                index++;
                }

            }
        }
    else
        { // TabBoxDesktopMode || TabBoxDesktopListMode
        int y = top;

        QRect r = workspace()->screenGeometry( workspace()->activeScreen());
        foreach (int it, desktops)
            {
            TabBoxDesktopItem* item = new TabBoxDesktopItem( it, this );
            item->setHeight( lineHeight );
            item->setWidth( width() - left - right );
            item->setPos( left, top + lineHeight * (it-1) );
            scene->addItem( item );
            // next desktop
            y += lineHeight;
            if ( y >= r.height() )
                break;
            }
        }
    }

/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev( bool next)
    {
    if ( mode() == TabBoxWindowsMode )
        {
        Client* firstClient = 0;
        Client* newClient = client;
        do
            {
            if ( next )
                newClient = workspace()->nextClientFocusChain(newClient);
            else
                newClient = workspace()->previousClientFocusChain(newClient);
            if (!firstClient)
                {
                // When we see our first client for the second time,
                // it's time to stop.
                firstClient = newClient;
                }
            else if (newClient == firstClient)
                {
                // No candidates found.
                newClient = 0;
                break;
                }
            } while ( newClient && !clients.contains( newClient ));
        setCurrentClient( newClient );
        }
    else if( mode() == TabBoxDesktopMode )
        {
        setCurrentDesktop ( next ? workspace()->nextDesktopFocusChain( desk )
                                 : workspace()->previousDesktopFocusChain( desk ) );
        }
    else
        { // TabBoxDesktopListMode
        setCurrentDesktop ( next ? workspace()->nextDesktopStatic( desk )
                                 : workspace()->previousDesktopStatic( desk )) ;
        }

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }



/*!
  Returns the currently displayed client ( only works in TabBoxWindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* TabBox::currentClient()
    {
    if ( mode() != TabBoxWindowsMode )
        return 0;
    if (!workspace()->hasClient( client ))
        return 0;
    return client;
    }

/*!
  Returns the list of clients potentially displayed ( only works in
  TabBoxWindowsMode ).
  Returns an empty list if no clients are available.
 */
ClientList TabBox::currentClientList()
    {
    if( mode() != TabBoxWindowsMode )
        return ClientList();
    return clients;
    }


/*!
  Returns the currently displayed virtual desktop ( only works in
  TabBoxDesktopListMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
    {
    if ( mode() == TabBoxDesktopListMode || mode() == TabBoxDesktopMode )
        return desk;
    return -1;
    }


/*!
  Returns the list of desktops potentially displayed ( only works in
  TabBoxDesktopListMode )
  Returns an empty list if no desktops are available.
 */
QList< int > TabBox::currentDesktopList()
    {
    if ( mode() == TabBoxDesktopListMode || mode() == TabBoxDesktopMode )
        return desktops;
    return QList< int >();
    }


/*!
  Change the currently selected client, and notify the effects.

  \sa setCurrentDesktop()
 */
void TabBox::setCurrentClient( Client* newClient )
    {
    foreach( QGraphicsItem *item, scene->items() )
        {
        TabBoxWindowItem* wItem = dynamic_cast<TabBoxWindowItem*>( item );
        if( !wItem )
            continue;

        if ( wItem->client() == newClient )
            {
            if (selectionItem)
                selectionItem->moveTo( wItem );
            else
                wItem->update();
            }
        else if ( !selectionItem && wItem->client() == client )
            {
                wItem->update();
            }
        }

    client = newClient;

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();

    if( isVisible() )
        updateOutline();
    }

/*!
  Change the currently selected desktop, and notify the effects.

  \sa setCurrentClient()
 */
void TabBox::setCurrentDesktop( int newDesktop )
    {
    foreach( QGraphicsItem *item, scene->items() )
        {
        TabBoxDesktopItem* dItem = dynamic_cast<TabBoxDesktopItem*>( item );
        if( !dItem )
            continue;

        if ( dItem->desktop() == newDesktop )
            {
            if (selectionItem)
                selectionItem->moveTo( dItem );
            else
                dItem->update();
            }
        else if ( !selectionItem && dItem->desktop() == desk )
            {
                dItem->update();
            }
        }

    desk = newDesktop;

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }

/*!
  Reimplemented to raise the tab box as well
 */
void TabBox::showEvent( QShowEvent* )
    {
    if( isVisible() )
        updateOutline();
    raise();
    }


/*!
  hide the icon box if necessary
 */
void TabBox::hideEvent( QHideEvent* )
    {
    }

void TabBox::drawBackground( QPainter* painter, const QRectF& rect )
    {
    painter->save();
    painter->setCompositionMode( QPainter::CompositionMode_Source );
    qreal left, top, right, bottom;
    frame.getMargins( left, top, right, bottom );
    frame.paintFrame( painter, rect.adjusted( -left, -top, right, bottom ) );
    painter->restore();
    }


/*!
  Notify effects that the tab box is being shown, and only display the
  default tab box QFrame if no effect has referenced the tab box.
*/
void TabBox::show()
    {
    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxAdded( mode());
    if( isDisplayed())
        return;
    refDisplay();
    QWidget::show();
    }

void TabBox::updateOutline()
    {
    Client* c = currentClient();
    if( c == NULL || this->isHidden() || !c->isShown( true ) || !c->isOnCurrentDesktop())
        {
        XUnmapWindow( QX11Info::display(), outline_left );
        XUnmapWindow( QX11Info::display(), outline_right );
        XUnmapWindow( QX11Info::display(), outline_top );
        XUnmapWindow( QX11Info::display(), outline_bottom );
        return;
        }
    // left/right parts are between top/bottom, they don't reach as far as the corners
    XMoveResizeWindow( QX11Info::display(), outline_left, c->x(), c->y() + 5, 5, c->height() - 10 );
    XMoveResizeWindow( QX11Info::display(), outline_right, c->x() + c->width() - 5, c->y() + 5, 5, c->height() - 10 );
    XMoveResizeWindow( QX11Info::display(), outline_top, c->x(), c->y(), c->width(), 5 );
    XMoveResizeWindow( QX11Info::display(), outline_bottom, c->x(), c->y() + c->height() - 5, c->width(), 5 );
    {
    QPixmap pix( 5, c->height() - 10 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 0, 0, 0, pix.height() - 1 );
    p.drawLine( 4, 0, 4, pix.height() - 1 );
    p.setPen( Qt::gray );
    p.drawLine( 1, 0, 1, pix.height() - 1 );
    p.drawLine( 3, 0, 3, pix.height() - 1 );
    p.setPen( Qt::black );
    p.drawLine( 2, 0, 2, pix.height() - 1 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outline_left, pix.handle());
    XSetWindowBackgroundPixmap( QX11Info::display(), outline_right, pix.handle());
    }
    {
    QPixmap pix( c->width(), 5 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 0, 0, pix.width() - 1 - 0, 0 );
    p.drawLine( 4, 4, pix.width() - 1 - 4, 4 );
    p.drawLine( 0, 0, 0, 4 );
    p.drawLine( pix.width() - 1 - 0, 0, pix.width() - 1 - 0, 4 );
    p.setPen( Qt::gray );
    p.drawLine( 1, 1, pix.width() - 1 - 1, 1 );
    p.drawLine( 3, 3, pix.width() - 1 - 3, 3 );
    p.drawLine( 1, 1, 1, 4 );
    p.drawLine( 3, 3, 3, 4 );
    p.drawLine( pix.width() - 1 - 1, 1, pix.width() - 1 - 1, 4 );
    p.drawLine( pix.width() - 1 - 3, 3, pix.width() - 1 - 3, 4 );
    p.setPen( Qt::black );
    p.drawLine( 2, 2, pix.width() - 1 - 2, 2 );
    p.drawLine( 2, 2, 2, 4 );
    p.drawLine( pix.width() - 1 - 2, 2, pix.width() - 1 - 2, 4 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outline_top, pix.handle());
    }
    {
    QPixmap pix( c->width(), 5 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 4, 0, pix.width() - 1 - 4, 0 );
    p.drawLine( 0, 4, pix.width() - 1 - 0, 4 );
    p.drawLine( 0, 4, 0, 0 );
    p.drawLine( pix.width() - 1 - 0, 4, pix.width() - 1 - 0, 0 );
    p.setPen( Qt::gray );
    p.drawLine( 3, 1, pix.width() - 1 - 3, 1 );
    p.drawLine( 1, 3, pix.width() - 1 - 1, 3 );
    p.drawLine( 3, 1, 3, 0 );
    p.drawLine( 1, 3, 1, 0 );
    p.drawLine( pix.width() - 1 - 3, 1, pix.width() - 1 - 3, 0 );
    p.drawLine( pix.width() - 1 - 1, 3, pix.width() - 1 - 1, 0 );
    p.setPen( Qt::black );
    p.drawLine( 2, 2, pix.width() - 1 - 2, 2 );
    p.drawLine( 2, 0, 2, 2 );
    p.drawLine( pix.width() - 1 - 2, 0, pix.width() - 1 - 2, 2 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outline_bottom, pix.handle());
    }
    XClearWindow( QX11Info::display(), outline_left );
    XClearWindow( QX11Info::display(), outline_right );
    XClearWindow( QX11Info::display(), outline_top );
    XClearWindow( QX11Info::display(), outline_bottom );
    XMapWindow( QX11Info::display(), outline_left );
    XMapWindow( QX11Info::display(), outline_right );
    XMapWindow( QX11Info::display(), outline_top );
    XMapWindow( QX11Info::display(), outline_bottom );
    }

/*!
  Notify effects that the tab box is being hidden.
*/
void TabBox::hide()
    {
    delayedShowTimer.stop();
    if( isVisible())
        {
        unrefDisplay();
        XUnmapWindow( QX11Info::display(), outline_left );
        XUnmapWindow( QX11Info::display(), outline_right );
        XUnmapWindow( QX11Info::display(), outline_top );
        XUnmapWindow( QX11Info::display(), outline_bottom );
        }
    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxClosed();
    if( isDisplayed())
        kDebug( 1212 ) << "Tab box was not properly closed by an effect";
    QWidget::hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (display(), EnterNotify, &otherEvent ) )
        ;
    }


/*!
  Decrease the reference count.  Only when the reference count is 0 will
  the default tab box be shown.
 */
void TabBox::unrefDisplay()
    {
    --display_refcount;
    }

void TabBox::reconfigure()
    {
    KSharedConfigPtr c(KGlobal::config());
    options_traverse_all = c->group("TabBox").readEntry("TraverseAll", false );
    }

/*!
  Rikkus: please document!   (Matthias)

  Ok, here's the docs :)

  You call delayedShow() instead of show() directly.

  If the 'ShowDelay' setting is false, show() is simply called.

  Otherwise, we start a timer for the delay given in the settings and only
  do a show() when it times out.

  This means that you can alt-tab between windows and you don't see the
  tab box immediately. Not only does this make alt-tabbing faster, it gives
  less 'flicker' to the eyes. You don't need to see the tab box if you're
  just quickly switching between 2 or 3 windows. It seems to work quite
  nicely.
 */
void TabBox::delayedShow()
    {
    KSharedConfigPtr c(KGlobal::config());
    KConfigGroup cg(c, "TabBox");
    bool delay = cg.readEntry("ShowDelay", true);

    if (!delay)
        {
        show();
        return;
        }

    int delayTime = cg.readEntry("DelayTime", 90);
    delayedShowTimer.setSingleShot(true);
    delayedShowTimer.start(delayTime);
    }


void TabBox::handleMouseEvent( XEvent* e )
    {
    XAllowEvents( display(), AsyncPointer, xTime());
    if( !isVisible() && isDisplayed())
        { // tabbox has been replaced, check effects
        if( effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent( e ))
            return;
        }
    if( e->type != ButtonPress )
        return;
    QPoint pos( e->xbutton.x_root, e->xbutton.y_root );
    QPoint widgetPos = mapFromGlobal( pos ); // inside tabbox

    if(( !isVisible() && isDisplayed())
        || !geometry().contains( pos ))
        {
        workspace()->closeTabBox();  // click outside closes tab
        return;
        }

    qreal left, top, right, bottom;
    frame.getMargins( left, top, right, bottom );
    int num = (widgetPos.y()-top) / lineHeight;

    if( mode() == TabBoxWindowsMode )
        {
        for( ClientList::ConstIterator it = clients.constBegin();
             it != clients.constEnd();
             ++it)
            {
            if( workspace()->hasClient( *it ) && (num == 0) ) // safety
                {
                setCurrentClient( *it );
                break;
                }
            num--;
            }
        }
    else
        {
        foreach( int it, desktops )
            {
            if( num == 0 )
                {
                setCurrentDesktop( it );
                break;
                }
            num--;
            }
        }
    }


//*******************************
// TabBoxSelectionItem
//*******************************
TabBoxSelectionItem::TabBoxSelectionItem( TabBox* parent )
    : QObject()
    , QGraphicsItem()
    , m_parent( parent )
    , m_timeLine( new QTimeLine( 100, this ) )
    {
    setCacheMode( DeviceCoordinateCache );
    setZValue( -1000 );
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsFocusable, false);

    m_timeLine->setFrameRange( 0, 40 );
    m_timeLine->setCurveShape( QTimeLine::EaseInCurve );
    connect( m_timeLine, SIGNAL( valueChanged(qreal) ),
             this, SLOT( animateMove(qreal) ) );
    }

TabBoxSelectionItem::~TabBoxSelectionItem()
    {
    }

QRectF TabBoxSelectionItem::boundingRect() const
    {
    return m_boundingRect;
    }

void TabBoxSelectionItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
    {
    Q_UNUSED( widget );
    painter->setRenderHint( QPainter::Antialiasing );
    m_parent->itemFrame()->paintFrame( painter, option->exposedRect, option->exposedRect );
    }

void TabBoxSelectionItem::moveTo( QGraphicsItem * item )
    {
    if (m_boundingRect != item->boundingRect())
        {
        prepareGeometryChange();
        m_boundingRect = item->boundingRect();
        }

    show();

    if (!m_animEndRect.isEmpty()) {
        setPos(m_animEndRect.topLeft());
    }

    m_animStartRect = QRectF( pos(), boundingRect().size() );
    m_animEndRect = QRectF( item->pos(), item->boundingRect().size() );
    m_timeLine->stop();
    m_timeLine->start();
    }

void TabBoxSelectionItem::animateMove( qreal t )
    {
        setPos(m_animStartRect.topLeft() * (1 - t) + m_animEndRect.topLeft() * t);
        if (qFuzzyCompare(t, qreal(1.0))) {
            m_animEndRect = QRect();
        }
    }

//*******************************
// TabBoxWindowItem
//*******************************

TabBoxWindowItem::TabBoxWindowItem( Client* client, TabBox* parent )
    : QGraphicsItem()
    , m_client( client )
    , m_parent( parent )
    , m_width( 0 )
    , m_height( 0 )
    , m_showMiniIcons( false )
    {
    setCacheMode( DeviceCoordinateCache );
    }

TabBoxWindowItem::~TabBoxWindowItem()
    {
    }

QRectF TabBoxWindowItem::boundingRect() const
    {
    return QRectF( 0, 0, m_width, m_height );
    }

void TabBoxWindowItem::setHeight( int height )
    {
    m_height = height;
    }

void TabBoxWindowItem::setWidth( int width )
    {
    m_width = width;
    }

void TabBoxWindowItem::setShowMiniIcons( bool showMiniIcons )
    {
    m_showMiniIcons = showMiniIcons;
    }

Client* TabBoxWindowItem::client() const
{
    return m_client;
}

void TabBoxWindowItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
    {
    painter->setRenderHint( QPainter::Antialiasing );

    drawBackground( painter, option, widget );

    // draw icon
    int iconWidth = m_showMiniIcons ? 16 : 32;
    int x = 0;
    QPixmap* menu_pix = kwin_get_menu_pix_hack();
    QPixmap icon;
    if ( m_showMiniIcons )
        {
        if ( !m_client->miniIcon().isNull() )
            icon = m_client->miniIcon();
        }
    else
        {
        if ( !m_client->icon().isNull() )
            icon = m_client->icon();
        else if ( menu_pix )
            icon = *menu_pix;
        }

    const int iconX = x + 5;
    const int iconY = ( m_height - iconWidth ) / 2;
    if( !icon.isNull() )
        {
        if( m_client->isMinimized())
            KIconEffect::semiTransparent( icon );
        if( m_client == m_parent->currentClient() )
            {
            KIconEffect *effect = KIconLoader::global()->iconEffect();
            icon = effect->apply( icon, KIconLoader::Desktop, KIconLoader::ActiveState );
            }
        painter->drawPixmap( iconX, iconY, icon );
        }

    // generate text to display
    QString s;

    if ( !m_client->isOnDesktop(m_parent->workspace()->currentDesktop()) )
        s = m_parent->workspace()->desktopName(m_client->desktop()).append(": ");

    int textOptions = Qt::AlignLeft | Qt::AlignBottom | Qt ::TextSingleLine;
    QFont font = painter->font();
    font.setPointSize( KGlobalSettings::smallestReadableFont().pointSize() );

    if ( m_client->isMinimized() )
        {
        s += '(' + m_client->caption() + ')';
        font.setItalic( true );
        }
    else
        s += m_client->caption();

    QFontMetrics fm = QFontMetrics( font );
    const int textWidth = m_width - iconX - iconWidth;
    int textHeight = m_height - (iconX * 2);

    if (!m_showMiniIcons && fm.height() < m_height * 2 / 3) {
        textHeight = m_height - (m_height / 3);
    }

    s = fm.elidedText( s, Qt::ElideMiddle, textWidth );
    painter->setPen( Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor ) );
    painter->setFont( font );
    painter->drawText( iconX + iconWidth + 8, iconY, textWidth, textHeight, textOptions, s );
    }

void TabBoxWindowItem::drawBackground( QPainter* painter, const QStyleOptionGraphicsItem* , QWidget* )
    {
    if( m_parent->itemsDrawBackgrounds() && m_client == m_parent->currentClient() )
        {
        m_parent->itemFrame()->paintFrame( painter, boundingRect() );
        }
    }


//*******************************
// TabBoxDesktopItem
//*******************************

TabBoxDesktopItem::TabBoxDesktopItem( int desktop, TabBox* parent )
    : QGraphicsItem()
    , m_desktop( desktop )
    , m_parent( parent )
    , m_width( 0 )
    , m_height( 0 )
    {
    setCacheMode( DeviceCoordinateCache );
    }

TabBoxDesktopItem::~TabBoxDesktopItem()
    {
    }

QRectF TabBoxDesktopItem::boundingRect() const
    {
    return QRectF( 0, 0, m_width, m_height );
    }

void TabBoxDesktopItem::setHeight( int height )
    {
    m_height = height;
    }

void TabBoxDesktopItem::setWidth( int width )
    {
    m_width = width;
    }

int TabBoxDesktopItem::desktop() const
    {
    return m_desktop;
    }

void TabBoxDesktopItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
    {
    painter->setRenderHint( QPainter::Antialiasing );

    drawBackground( painter, option, widget );

    int iconWidth = 32;
    int iconHeight = iconWidth;
    int x = 0;
    int y = 0;

    // get widest desktop name/number
    QFont f = painter->font();
    f.setBold(true);
    f.setPixelSize(iconHeight - 4);  // pixel, not point because I need to know the pixels
    QFontMetrics fm = QFontMetrics( f );

    QString num = QString::number( m_desktop );
    iconWidth = qMax(iconWidth - 4, fm.boundingRect(num).width()) + 4;

    // draw "icon" (here: number of desktop)
    painter->save();
    QBrush brush;
    if( m_desktop == m_parent->currentDesktop() )
        brush = m_parent->palette().brush( QPalette::Active, QPalette::Highlight );
    else
        brush = m_parent->palette().brush( QPalette::Active, QPalette::Base );
    painter->fillRect(x+5, y+4, iconWidth, iconHeight, brush );
    painter->setPen(m_parent->palette().color( QPalette::Active, QPalette::Text ));
    painter->drawRect(x+5, y+4, iconWidth, iconHeight);

    // draw desktop-number
    painter->setFont(f);
    painter->drawText(x+5, y+4, iconWidth, iconHeight, Qt::AlignCenter | Qt::AlignVCenter, num);

    painter->restore();

    // draw desktop name text
    QFont font = painter->font();
    font.setPointSize( KGlobalSettings::smallestReadableFont().pointSize() );
    painter->setFont( font );
    fm = QFontMetrics( font );
    int wmax = fm.width( m_parent->workspace()->desktopName( m_desktop ));

    painter->setPen( Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor ) );
    painter->drawText(x+5 + iconWidth + 8, y, m_width - 5 - iconWidth - 8, m_height - 4,
                    Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine,
                    m_parent->workspace()->desktopName( m_desktop ));

    // show mini icons from that desktop aligned to each other
    int x1 = x + 5 + iconWidth + 8 + wmax + 5;

    ClientList list;
    m_parent->createClientList(list, m_desktop, 0, false);
    // clients are in reversed stacking order
    for ( int i = list.size() - 1; i>=0; i-- )
        {
        if ( !list.at( i )->miniIcon().isNull() )
            {
            if ( x1+18 >= m_width )  // only show full icons
                break;

            painter->drawPixmap( x1, y + (m_height - 16)/2, list.at(  i )->miniIcon() );
            x1 += 18;
            }
        }
    }

void TabBoxDesktopItem::drawBackground( QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* )
    {
    if( m_parent->itemsDrawBackgrounds() && m_desktop == m_parent->currentDesktop() )
        {
        m_parent->itemFrame()->paintFrame( painter, boundingRect() );
        }
    }


//*******************************
// Workspace
//*******************************


/*!
  Handles alt-tab / control-tab
 */

static
bool areKeySymXsDepressed( bool bAll, const uint keySyms[], int nKeySyms )
    {
    char keymap[32];

    kDebug(125) << "areKeySymXsDepressed: " << (bAll ? "all of " : "any of ") << nKeySyms;

    XQueryKeymap( display(), keymap );

    for( int iKeySym = 0; iKeySym < nKeySyms; iKeySym++ )
        {
        uint keySymX = keySyms[ iKeySym ];
        uchar keyCodeX = XKeysymToKeycode( display(), keySymX );
        int i = keyCodeX / 8;
        char mask = 1 << (keyCodeX - (i * 8));

                // Abort if bad index value,
        if( i < 0 || i >= 32 )
                return false;

        kDebug(125) << iKeySym << ": keySymX=0x" << QString::number( keySymX, 16 )
                << " i=" << i << " mask=0x" << QString::number( mask, 16 )
                << " keymap[i]=0x" << QString::number( keymap[i], 16 ) << endl;

                // If ALL keys passed need to be depressed,
        if( bAll )
            {
            if( (keymap[i] & mask) == 0 )
                    return false;
            }
        else
            {
                        // If we are looking for ANY key press, and this key is depressed,
            if( keymap[i] & mask )
                    return true;
            }
        }

        // If we were looking for ANY key press, then none was found, return false,
        // If we were looking for ALL key presses, then all were found, return true.
    return bAll;
    }

static bool areModKeysDepressed( const QKeySequence& seq )
    {
    uint rgKeySyms[10];
    int nKeySyms = 0;
    if( seq.isEmpty())
        return false;
    int mod = seq[seq.count()-1] & Qt::KeyboardModifierMask;

    if ( mod & Qt::SHIFT )
        {
        rgKeySyms[nKeySyms++] = XK_Shift_L;
        rgKeySyms[nKeySyms++] = XK_Shift_R;
        }
    if ( mod & Qt::CTRL )
        {
        rgKeySyms[nKeySyms++] = XK_Control_L;
        rgKeySyms[nKeySyms++] = XK_Control_R;
        }
    if( mod & Qt::ALT )
        {
        rgKeySyms[nKeySyms++] = XK_Alt_L;
        rgKeySyms[nKeySyms++] = XK_Alt_R;
        }
    if( mod & Qt::META )
        {
        // It would take some code to determine whether the Win key
        // is associated with Super or Meta, so check for both.
        // See bug #140023 for details.
        rgKeySyms[nKeySyms++] = XK_Super_L;
        rgKeySyms[nKeySyms++] = XK_Super_R;
        rgKeySyms[nKeySyms++] = XK_Meta_L;
        rgKeySyms[nKeySyms++] = XK_Meta_R;
        }

    return areKeySymXsDepressed( false, rgKeySyms, nKeySyms );
    }

static bool areModKeysDepressed( const KShortcut& cut )
    {
    if( areModKeysDepressed( cut.primary()) || areModKeysDepressed( cut.alternate()) )
        return true;

    return false;
    }

void Workspace::slotWalkThroughWindows()
    {
    if ( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE || !options->focusPolicyIsReasonable())
        {
        //ungrabXKeyboard(); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows( true );
        }
    else
        {
        if ( areModKeysDepressed( cutWalkThroughWindows ) )
            {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( true );
            }
        else
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            KDEOneStepThroughWindows( true );
        }
    }

void Workspace::slotWalkBackThroughWindows()
    {
    if( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE || !options->focusPolicyIsReasonable())
        {
        // CDE style raise / lower
        CDEWalkThroughWindows( false );
        }
    else
        {
        if ( areModKeysDepressed( cutWalkThroughWindowsReverse ) )
            {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( false );
            }
        else
            {
            KDEOneStepThroughWindows( false );
            }
        }
    }

void Workspace::slotWalkThroughDesktops()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktops ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktops( true );
        }
    }

void Workspace::slotWalkBackThroughDesktops()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopsReverse ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktops( false );
        }
    }

void Workspace::slotWalkThroughDesktopList()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopList ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktopList( true );
        }
    }

void Workspace::slotWalkBackThroughDesktopList()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopListReverse ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktopList( false );
        }
    }

void Workspace::slotWalkThroughDesktopsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktops = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughDesktopsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopsReverse = KShortcut( seq );
    }

void Workspace::slotWalkThroughDesktopListKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopList = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughDesktopListKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopListReverse = KShortcut( seq );
    }

void Workspace::slotWalkThroughWindowsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindows = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughWindowsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindowsReverse = KShortcut( seq );
    }

void Workspace::modalActionsSwitch( bool enabled )
    {
    QList<KActionCollection*> collections;
    collections.append( keys );
    collections.append( disable_shortcuts_keys );
    collections.append( client_keys );
    foreach (KActionCollection* collection, collections)
        foreach (QAction *action, collection->actions())
            action->setEnabled(enabled);
    }

bool Workspace::startKDEWalkThroughWindows()
    {
    if( !establishTabBoxGrab())
        return false;
    tab_grab = true;
    modalActionsSwitch( false );
    tab_box->setMode( TabBoxWindowsMode );
    tab_box->reset();
    tab_box->initScene();
    return true;
    }

bool Workspace::startWalkThroughDesktops( TabBoxMode mode )
    {
    if( !establishTabBoxGrab())
        return false;
    control_grab = true;
    modalActionsSwitch( false );
    tab_box->setMode( mode );
    tab_box->reset();
    tab_box->initScene();
    return true;
    }

bool Workspace::startWalkThroughDesktops()
    {
    return startWalkThroughDesktops( TabBoxDesktopMode );
    }

bool Workspace::startWalkThroughDesktopList()
    {
    return startWalkThroughDesktops( TabBoxDesktopListMode );
    }

void Workspace::KDEWalkThroughWindows( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::walkThroughDesktops( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::CDEWalkThroughWindows( bool forward )
    {
    Client* c = NULL;
// this function find the first suitable client for unreasonable focus
// policies - the topmost one, with some exceptions (can't be keepabove/below,
// otherwise it gets stuck on them)
    Q_ASSERT( block_stacking_updates == 0 );
    for( int i = stacking_order.size() - 1;
         i >= 0 ;
         --i )
        {
        Client* it = stacking_order.at( i );
        if ( it->isOnCurrentDesktop() && !it->isSpecialWindow()
            && it->isShown( false ) && it->wantsTabFocus()
            && !it->keepAbove() && !it->keepBelow())
            {
            c = it;
            break;
            }
        }
    Client* nc = c;
    bool options_traverse_all;
        {
        KConfigGroup group( KGlobal::config(), "TabBox" );
        options_traverse_all = group.readEntry("TraverseAll", false );
        }

    Client* firstClient = 0;
    do
        {
        nc = forward ? nextClientStatic(nc) : previousClientStatic(nc);
        if (!firstClient)
            {
            // When we see our first client for the second time,
            // it's time to stop.
            firstClient = nc;
            }
        else if (nc == firstClient)
            {
            // No candidates found.
            nc = 0;
            break;
            }
        } while (nc && nc != c &&
            (( !options_traverse_all && !nc->isOnDesktop(currentDesktop())) ||
             nc->isMinimized() || !nc->wantsTabFocus() || nc->keepAbove() || nc->keepBelow() ) );
    if (nc)
        {
        if (c && c != nc)
            lowerClient( c );
        if ( options->focusPolicyIsReasonable() )
            {
            activateClient( nc );
            if( nc->isShade() && options->shadeHover )
                nc->setShade( ShadeActivated );
            }
        else
            {
            if( !nc->isOnDesktop( currentDesktop()))
                setCurrentDesktop( nc->desktop());
            raiseClient( nc );
            }
        }
    }

void Workspace::KDEOneStepThroughWindows( bool forward )
    {
    tab_box->setMode( TabBoxWindowsMode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if( Client* c = tab_box->currentClient() )
        {
        activateClient( c );
        if( c->isShade() && options->shadeHover )
            c->setShade( ShadeActivated );
        }
    }

void Workspace::oneStepThroughDesktops( bool forward, TabBoxMode mode )
    {
    tab_box->setMode( mode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if ( tab_box->currentDesktop() != -1 )
        setCurrentDesktop( tab_box->currentDesktop() );
    }

void Workspace::oneStepThroughDesktops( bool forward )
    {
    oneStepThroughDesktops( forward, TabBoxDesktopMode );
    }

void Workspace::oneStepThroughDesktopList( bool forward )
    {
    oneStepThroughDesktops( forward, TabBoxDesktopListMode );
    }

/*!
  Handles holding alt-tab / control-tab
 */
void Workspace::tabBoxKeyPress( int keyQt )
    {
    bool forward = false;
    bool backward = false;

    if (tab_grab)
        {
        forward = cutWalkThroughWindows.contains( keyQt );
        backward = cutWalkThroughWindowsReverse.contains( keyQt );
        if (forward || backward)
            {
            kDebug(125) << "== " << cutWalkThroughWindows.toString()
                << " or " << cutWalkThroughWindowsReverse.toString() << endl;
            KDEWalkThroughWindows( forward );
            }
        }
    else if (control_grab)
        {
        forward = cutWalkThroughDesktops.contains( keyQt ) ||
                  cutWalkThroughDesktopList.contains( keyQt );
        backward = cutWalkThroughDesktopsReverse.contains( keyQt ) ||
                   cutWalkThroughDesktopListReverse.contains( keyQt );
        if (forward || backward)
            walkThroughDesktops(forward);
        }

    if (control_grab || tab_grab)
        {
        if ( ((keyQt & ~Qt::KeyboardModifierMask) == Qt::Key_Escape)
            && !(forward || backward) )
            { // if Escape is part of the shortcut, don't cancel
            closeTabBox();
            }
        }
    }

void Workspace::refTabBox()
    {
    if( tab_box )
        tab_box->refDisplay();
    }

void Workspace::unrefTabBox()
    {
    if( tab_box )
        tab_box->unrefDisplay();
    }

void Workspace::closeTabBox()
    {
    removeTabBoxGrab();
    tab_box->hide();
    modalActionsSwitch( true );
    tab_grab = false;
    control_grab = false;
    }

/*!
  Handles alt-tab / control-tab releasing
 */
void Workspace::tabBoxKeyRelease( const XKeyEvent& ev )
    {
    unsigned int mk = ev.state &
        (KKeyServer::modXShift() |
         KKeyServer::modXCtrl() |
         KKeyServer::modXAlt() |
         KKeyServer::modXMeta() );
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for( int i = ShiftMapIndex;
         i <= Mod5MapIndex;
         ++i )
        if(( mk & ( 1 << i )) != 0 )
        {
        if( mod_index >= 0 )
            return;
        mod_index = i;
        }
    bool release = false;
    if( mod_index == -1 )
        release = true;
    else
        {
        XModifierKeymap* xmk = XGetModifierMapping(display());
        for (int i=0; i<xmk->max_keypermod; i++)
            if (xmk->modifiermap[xmk->max_keypermod * mod_index + i]
                == ev.keycode)
                release = true;
        XFreeModifiermap(xmk);
        }
    if( !release )
         return;
    if (tab_grab)
        {
        bool old_control_grab = control_grab;
        closeTabBox();
        control_grab = old_control_grab;
        if( Client* c = tab_box->currentClient())
            {
            activateClient( c );
            if( c->isShade() && options->shadeHover )
                c->setShade( ShadeActivated );
            }
        }
    if (control_grab)
        {
        bool old_tab_grab = tab_grab;
        closeTabBox();
        tab_grab = old_tab_grab;
        if ( tab_box->currentDesktop() != -1 )
            {
            setCurrentDesktop( tab_box->currentDesktop() );
            }
        }
    }


int Workspace::nextDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i >= 0 && i+1 < (int)desktop_focus_chain.size() )
            return desktop_focus_chain[i+1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[ 0 ];
    else
            return 1;
    }

int Workspace::previousDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i-1 >= 0 )
            return desktop_focus_chain[i-1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[desktop_focus_chain.size()-1];
    else
            return numberOfDesktops();
    }

int Workspace::nextDesktopStatic( int iDesktop ) const
    {
    int i = ++iDesktop;
    if( i > numberOfDesktops())
        i = 1;
    return i;
    }

int Workspace::previousDesktopStatic( int iDesktop ) const
    {
    int i = --iDesktop;
    if( i < 1 )
        i = numberOfDesktops();
    return i;
    }

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::nextClientFocusChain( Client* c ) const
    {
    if ( global_focus_chain.isEmpty() )
        return 0;
    int pos = global_focus_chain.indexOf( c );
    if ( pos == -1 )
        return global_focus_chain.last();
    if ( pos == 0 )
        return global_focus_chain.last();
    pos--;
    return global_focus_chain[ pos ];
    }

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::previousClientFocusChain( Client* c ) const
    {
    if ( global_focus_chain.isEmpty() )
        return 0;
    int pos = global_focus_chain.indexOf( c );
    if ( pos == -1 )
        return global_focus_chain.first();
    pos++;
    if ( pos == global_focus_chain.count() )
        return global_focus_chain.first();
    return global_focus_chain[ pos ];
    }

/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::nextClientStatic( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    int pos = clients.indexOf( c );
    if ( pos == -1 )
        return clients.first();
    ++pos;
    if ( pos == clients.count() )
        return clients.first();
    return clients[ pos ];
    }
/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::previousClientStatic( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    int pos = clients.indexOf( c );
    if ( pos == -1 )
        return clients.last();
    if ( pos == 0 )
        return clients.last();
    --pos;
    return clients[ pos ];
    }

Client* Workspace::currentTabBoxClient() const
    {
    if( !tab_box )
        return 0;
    return tab_box->currentClient();
    }

ClientList Workspace::currentTabBoxClientList() const
    {
    if( !tab_box )
        return ClientList();
    return tab_box->currentClientList();
    }

int Workspace::currentTabBoxDesktop() const
    {
    if( !tab_box )
        return -1;
    return tab_box->currentDesktop();
    }

QList< int > Workspace::currentTabBoxDesktopList() const
    {
    if( !tab_box )
        return QList< int >();
    return tab_box->currentDesktopList();
    }

void Workspace::setTabBoxClient( Client* c )
    {
    if( tab_box )
        tab_box->setCurrentClient( c );
    }

void Workspace::setTabBoxDesktop( int iDesktop )
    {
    if( tab_box )
        tab_box->setCurrentDesktop( iDesktop );
    }

bool Workspace::establishTabBoxGrab()
    {
    if( !grabXKeyboard())
        return false;
    // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
    // using Alt+Tab while DND (#44972). However force passive grabs on all windows
    // in order to catch MouseRelease events and close the tabbox (#67416).
    // All clients already have passive grabs in their wrapper windows, so check only
    // the active client, which may not have it.
    assert( !forced_global_mouse_grab );
    forced_global_mouse_grab = true;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    return true;
    }

void Workspace::removeTabBoxGrab()
    {
    ungrabXKeyboard();
    assert( forced_global_mouse_grab );
    forced_global_mouse_grab = false;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    }

} // namespace

#include "tabbox.moc"
