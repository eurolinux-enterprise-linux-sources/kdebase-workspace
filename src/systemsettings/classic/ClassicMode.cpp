/**************************************************************************
 * Copyright (C) 2009 Ben Cooksley <ben@eclipse.endoftheinternet.org>     *
 * Copyright (C) 2008 Mathias Soeken <msoeken@informatik.uni-bremen.de>   *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 2         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program; if not, write to the Free Software            *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA          *
 * 02110-1301, USA.                                                       *
***************************************************************************/

#include "ClassicMode.h"

#include <QLayout>
#include <QSplitter>
#include <QTreeView>
#include <QModelIndex>
#include <QStackedWidget>
#include <QAbstractItemModel>

#include <KDialog>
#include <KAboutData>
#include <KCModuleInfo>
#include <KGlobalSettings>

#include "MenuItem.h"
#include "MenuModel.h"
#include "ModuleView.h"
#include "CategoryList.h"
#include "MenuProxyModel.h"

K_PLUGIN_FACTORY(ClassicModeFactory, registerPlugin<ClassicMode>();)
K_EXPORT_PLUGIN(ClassicModeFactory("classic_mode"))

class ClassicMode::Private {
public:
    Private() : moduleView( 0 ) {}
    virtual ~Private() {
        delete aboutClassic;
    }

    QSplitter * classicWidget;
    QTreeView * classicTree;
    CategoryList * classicCategory;
    QStackedWidget * stackedWidget;
    ModuleView * moduleView;

    MenuProxyModel * proxyModel;
    MenuModel * model;
    KAboutData * aboutClassic;
};

ClassicMode::ClassicMode( QObject * parent, const QVariantList& )
    : BaseMode( parent ), d( new Private() )
{
    d->aboutClassic = new KAboutData( "TreeView", 0, ki18n("Tree View"),
                                   "1.0", ki18n("Provides a classic tree-based view of control modules."),
                                   KAboutData::License_GPL, ki18n("(c) 2009, Ben Cooksley"));
    d->aboutClassic->addAuthor(ki18n("Ben Cooksley"), ki18n("Author"), "ben@eclipse.endoftheinternet.org");
    d->aboutClassic->addAuthor(ki18n("Mathias Soeken"), ki18n("Developer"), "msoeken@informatik.uni-bremen.de");
    d->aboutClassic->setProgramIconName("view-list-tree");
}

ClassicMode::~ClassicMode()
{
    if( !d->classicTree ) {
        delete d->classicWidget;
    }
    delete d;
}

void ClassicMode::initEvent()
{
    // Create the model
    d->model = new MenuModel( rootItem(), this );
    foreach( MenuItem * child, rootItem()->children() ) {
        d->model->addException( child );
    }
    // Create the model
    d->proxyModel = new MenuProxyModel( this );
    d->proxyModel->setSourceModel( d->model );
    d->proxyModel->sort( 0 );
    d->classicWidget = new QSplitter( Qt::Horizontal, 0 );
    d->classicWidget->setChildrenCollapsible( false );
    d->moduleView = new ModuleView( d->classicWidget );
    d->classicTree = 0;
}

QWidget * ClassicMode::mainWidget()
{
    if( !d->classicTree ) {
        initWidget();
    }
    return d->classicWidget;
}

KAboutData * ClassicMode::aboutData()
{
    return d->aboutClassic;
}

ModuleView * ClassicMode::moduleView() const
{
    return d->moduleView;
}

QList<QAbstractItemView*> ClassicMode::views() const
{
    QList<QAbstractItemView*> theViews;
    theViews << d->classicTree;
    return theViews;
}

void ClassicMode::saveState()
{
    config().writeEntry( "viewLayout", d->classicWidget->sizes() );
    config().sync();
}

void ClassicMode::expandColumns()
{
    d->classicTree->resizeColumnToContents(0);
}

void ClassicMode::searchChanged( const QString& text )
{
    d->proxyModel->setFilterRegExp(text);
    if( d->classicTree ) {
        d->classicCategory->changeModule( d->classicTree->currentIndex() );
    }
}

void ClassicMode::selectModule( const QModelIndex& selectedModule )
{
    d->classicTree->setCurrentIndex( selectedModule );
    if( d->proxyModel->rowCount(selectedModule) > 0 ) {
        d->classicTree->setExpanded(selectedModule, true);
    }
    changeModule( selectedModule );
}

void ClassicMode::changeModule( const QModelIndex& activeModule )
{
    if( !d->moduleView->resolveChanges() ) {
        return;
    }
    d->moduleView->closeModules();
    if( d->proxyModel->rowCount(activeModule) > 0 ) {
        d->stackedWidget->setCurrentWidget( d->classicCategory );
        d->classicCategory->changeModule(activeModule);
        emit viewChanged( false );
    } else {
        MenuItem *menuItem = d->proxyModel->data( activeModule, Qt::UserRole ).value<MenuItem*>();
        d->moduleView->loadModule( menuItem );
    }
}

void ClassicMode::moduleLoaded()
{
    d->stackedWidget->setCurrentWidget( d->moduleView );
}

void ClassicMode::initWidget()
{
    // Create the widget
    d->classicTree = new QTreeView( d->classicWidget );
    d->classicCategory = new CategoryList( d->classicWidget, d->proxyModel );

    d->stackedWidget = new QStackedWidget( d->classicWidget );
    d->stackedWidget->layout()->setMargin(0);
    d->stackedWidget->addWidget( d->classicCategory );
    d->stackedWidget->addWidget( d->moduleView );

    d->classicWidget->addWidget( d->classicTree );
    d->classicWidget->addWidget( d->stackedWidget );

    d->classicTree->setModel( d->proxyModel );
    d->classicTree->setHeaderHidden( true );
    d->classicTree->setIconSize( QSize( 24, 24 ) );
    d->classicTree->setSortingEnabled( true );
    d->classicTree->setMouseTracking( true );
    d->classicTree->setMinimumWidth( 200 );
    d->classicTree->setSelectionMode(QAbstractItemView::SingleSelection);
    d->classicTree->sortByColumn( 0, Qt::AscendingOrder );

    d->classicCategory->changeModule( d->classicTree->rootIndex() );

    connect( d->classicCategory, SIGNAL(moduleSelected( QModelIndex )), this, SLOT(selectModule( QModelIndex )));
    connect( d->classicTree, SIGNAL(activated(const QModelIndex&)), this, SLOT(changeModule(const QModelIndex&)));
    connect( d->classicTree, SIGNAL(collapsed(QModelIndex)), this, SLOT(expandColumns()));
    connect( d->classicTree, SIGNAL(expanded(QModelIndex)), this, SLOT(expandColumns()));
    connect( d->moduleView, SIGNAL( moduleChanged(bool) ), this, SLOT( moduleLoaded() ) );

    if( !KGlobalSettings::singleClick() ) {
        // Needed because otherwise activated() is not fired with single click, which is apparently expected for tree views
        connect( d->classicTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(changeModule(const QModelIndex&)) );
    }

    expandColumns();
    QList<int> defaultSizes;
    defaultSizes << 250 << 500;
    d->classicWidget->setSizes( config().readEntry( "viewLayout", defaultSizes ) );
}

void ClassicMode::leaveModuleView()
{
    d->moduleView->closeModules();
    d->stackedWidget->setCurrentWidget( d->classicCategory );
}

#include "ClassicMode.moc"
