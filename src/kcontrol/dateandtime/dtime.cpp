/*
 *  dtime.cpp
 *
 *  Copyright (C) 1998 Luca Montecchiani <m.luca@usa.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include "dtime.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#include <QComboBox>
#include <QtGui/QGroupBox>
#include <QPushButton>
#include <QPainter>
#include <QLayout>
#include <QLabel>

#include <QCheckBox>
#include <QRegExp>
#include <QPaintEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QPolygon>
#include <kdebug.h>
#include <klocale.h>
#include <kprocess.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kdialog.h>
#include <kconfig.h>
#include <kcolorscheme.h>

#include "dtime.moc"

#include "helper.h"

HMSTimeWidget::HMSTimeWidget(QWidget *parent) :
	KIntSpinBox(parent)
{
}

QString HMSTimeWidget::mapValueToText(int value)
{
  QString s = QString::number(value);
  if( value < 10 ) {
    s = '0' + s;
  }
  return s;
}

Dtime::Dtime(QWidget * parent)
  : QWidget(parent)
{
  // *************************************************************
  // Start Dialog
  // *************************************************************

  // Time Server

  privateLayoutWidget = new QWidget( this );
  QHBoxLayout *layout1 = new QHBoxLayout( privateLayoutWidget );
  layout1->setObjectName( "ntplayout" );
  layout1->setSpacing( 0 );
  layout1->setMargin( 0 );

  setDateTimeAuto = new QCheckBox( privateLayoutWidget );
  setDateTimeAuto->setObjectName( "setDateTimeAuto" );
  setDateTimeAuto->setText(i18n("Set date and time &automatically:"));
  connect(setDateTimeAuto, SIGNAL(toggled(bool)), this, SLOT(serverTimeCheck()));
  connect(setDateTimeAuto, SIGNAL(toggled(bool)), SLOT(configChanged()));
  layout1->addWidget( setDateTimeAuto );

  timeServerList = new QComboBox( privateLayoutWidget );
  timeServerList->setObjectName( "timeServerList" );
  timeServerList->setEditable(false);
  connect(timeServerList, SIGNAL(activated(int)), SLOT(configChanged()));
  connect(timeServerList, SIGNAL(editTextChanged(const QString &)), SLOT(configChanged()));
  connect(setDateTimeAuto, SIGNAL(toggled(bool)), timeServerList, SLOT(setEnabled(bool)));
  timeServerList->setEnabled(false);
  timeServerList->setEditable(true);
  layout1->addWidget( timeServerList );
  findNTPutility();

  // Date box
  QGroupBox* dateBox = new QGroupBox( this );
  dateBox->setObjectName( QLatin1String( "dateBox" ) );

  QVBoxLayout *l1 = new QVBoxLayout( dateBox );
  l1->setMargin( 0 );
  l1->setSpacing( KDialog::spacingHint() );

  cal = new KDatePicker( dateBox );
  cal->setMinimumSize(cal->sizeHint());
  l1->addWidget( cal );
  cal->setWhatsThis( i18n("Here you can change the system date's day of the month, month and year.") );

  // Time frame
  QGroupBox* timeBox = new QGroupBox( this );
  timeBox->setObjectName( QLatin1String( "timeBox" ) );

  QVBoxLayout *v2 = new QVBoxLayout( timeBox );
  v2->setMargin( 0 );
  v2->setSpacing( KDialog::spacingHint() );

  kclock = new Kclock( timeBox );
  kclock->setObjectName("Kclock");
  kclock->setMinimumSize(150,150);
  v2->addWidget( kclock );

  QGridLayout *v3 = new QGridLayout( );
  v2->addItem( v3 );

  // Even if the module's widgets are reversed (usually when using RTL
  // languages), the placing of the time fields must always be H:M:S, from
  // left to right.
  bool isRTL = QApplication::isRightToLeft();

  QSpacerItem *spacer1 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
  v3->addItem(spacer1, 0, 1, 2, 1);

  hour = new HMSTimeWidget( timeBox );
  hour->setWrapping(true);
  hour->setMaximum(23);
#ifdef __GNUC__
#warning fixme hour->setValidator(new KStrictIntValidator(0, 23, hour));
#endif
  v3->addWidget(hour, 0, isRTL ? 6 : 2, 2, 1);

  QLabel *dots1 = new QLabel(":", timeBox);
  dots1->setMinimumWidth( 7 );
  dots1->setAlignment( Qt::AlignCenter );
  v3->addWidget(dots1, 0, 3, 2, 1);

  minute = new HMSTimeWidget( timeBox );
  minute->setWrapping(true);
  minute->setMinimum(0);
  minute->setMaximum(59);
#ifdef __GNUC__
  #warning fixme minute->setValidator(new KStrictIntValidator(0, 59, minute));
#endif
  v3->addWidget(minute, 0, 4, 2, 1);

  QLabel *dots2 = new QLabel(":", timeBox);
  dots2->setMinimumWidth( 7 );
  dots2->setAlignment( Qt::AlignCenter );
  v3->addWidget(dots2, 0, 5, 2, 1);

  second = new HMSTimeWidget( timeBox );
  second->setWrapping(true);
  second->setMinimum(0);
  second->setMaximum(59);
#ifdef __GNUC__
  #warning fixme second->setValidator(new KStrictIntValidator(0, 59, second));
#endif
  v3->addWidget(second, 0, isRTL ? 2 : 6, 2, 1);

  v3->addItem(new QSpacerItem(7, 0), 0, 7);

  QString wtstr = i18n("Here you can change the system time. Click into the"
    " hours, minutes or seconds field to change the relevant value, either"
    " using the up and down buttons to the right or by entering a new value.");
  hour->setWhatsThis( wtstr );
  minute->setWhatsThis( wtstr );
  second->setWhatsThis( wtstr );

  QSpacerItem *spacer3 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
  v3->addItem(spacer3, 0, 9, 2, 1);

  QGridLayout *top = new QGridLayout( this );
  top->setMargin( 0 );
  top->setSpacing( KDialog::spacingHint() );

  top->addWidget(dateBox, 1,0);
  top->addWidget(timeBox, 1,1);
  top->addWidget(privateLayoutWidget, 0, 0, 1, 2 );

  // *************************************************************
  // End Dialog
  // *************************************************************

  connect( hour, SIGNAL(valueChanged(int)), SLOT(set_time()) );
  connect( minute, SIGNAL(valueChanged(int)), SLOT(set_time()) );
  connect( second, SIGNAL(valueChanged(int)), SLOT(set_time()) );
  connect( cal, SIGNAL(dateChanged(QDate)), SLOT(changeDate(QDate)));

  connect( &internalTimer, SIGNAL(timeout()), SLOT(timeout()) );

  kclock->setEnabled(false);
}

void Dtime::serverTimeCheck() {
  bool enabled = !setDateTimeAuto->isChecked();
  cal->setEnabled(enabled);
  hour->setEnabled(enabled);
  minute->setEnabled(enabled);
  second->setEnabled(enabled);
  //kclock->setEnabled(enabled);
}

void Dtime::findNTPutility(){
  if(!KStandardDirs::findExe("ntpdate").isEmpty()) {
    ntpUtility = "ntpdate";
    kDebug() << "ntpUtility = " << ntpUtility;
    return;
  }
  if(!KStandardDirs::findExe("rdate").isEmpty()) {
    ntpUtility = "rdate";
    kDebug() << "ntpUtility = " << ntpUtility;
    return;
  }
  privateLayoutWidget->hide();
  kDebug() << "ntpUtility not found!";
}

void Dtime::set_time()
{
  if( ontimeout )
    return;

  internalTimer.stop();

  time.setHMS( hour->value(), minute->value(), second->value() );
  kclock->setTime( time );

  emit timeChanged( true );
}

void Dtime::changeDate(const QDate &d)
{
  date = d;
  emit timeChanged( true );
}

void Dtime::configChanged(){
  emit timeChanged( true );
}

void Dtime::load()
{
  // The config is actually written to the system config, but the user does not have any local config,
  // since there is nothing writing it.
  KConfig _config( "kcmclockrc", KConfig::NoGlobals );
  KConfigGroup config(&_config, "NTP");
  timeServerList->clear();
  timeServerList->addItems(config.readEntry("servers",
    i18n("Public Time Server (pool.ntp.org),\
asia.pool.ntp.org,\
europe.pool.ntp.org,\
north-america.pool.ntp.org,\
oceania.pool.ntp.org")).split(',', QString::SkipEmptyParts));
  setDateTimeAuto->setChecked(config.readEntry("enabled", false));

  // Reset to the current date and time
  time = QTime::currentTime();
  date = QDate::currentDate();
  cal->setDate(date);

  // start internal timer
  internalTimer.start( 1000 );

  timeout();
}

void Dtime::save( QStringList& helperargs )
{
  // Save the order, but don't duplicate!
  QStringList list;
  if( timeServerList->count() != 0)
    list.append(timeServerList->currentText());
  for ( int i=0; i<timeServerList->count();i++ ) {
    QString text = timeServerList->itemText(i);
    if( !list.contains(text) )
      list.append(text);
    // Limit so errors can go away and not stored forever
    if( list.count() == 10)
      break;
  }
  helperargs << "ntp" << QString::number( list.count()) << list
      << ( setDateTimeAuto->isChecked() ? "enabled" : "disabled" );

  if(setDateTimeAuto->isChecked() && !ntpUtility.isEmpty()){
    // NTP Time setting - done in helper
    timeServer = timeServerList->currentText();
    kDebug() << "Setting date from time server " << timeServer;
  }
  else {
    // User time setting
    QDateTime dt(date,
        QTime(hour->value(), minute->value(), second->value()));

    kDebug() << "Set date " << dt;

    helperargs << "date" << QString::number(dt.toTime_t())
                         << QString::number(::time(0));
  }

  // restart time
  internalTimer.start( 1000 );
}

void Dtime::processHelperErrors( int code )
{
  if( code & ERROR_DTIME_NTP ) {
    KMessageBox::error( this, i18n("Unable to contact time server: %1.", timeServer) );
    setDateTimeAuto->setChecked( false );
  }
  if( code & ERROR_DTIME_DATE ) {
    KMessageBox::error( this, i18n("Can not set date."));
  }
}

void Dtime::timeout()
{
  // get current time
  time = QTime::currentTime();

  ontimeout = true;
  second->setValue(time.second());
  minute->setValue(time.minute());
  hour->setValue(time.hour());
  ontimeout = false;

  kclock->setTime( time );
}

QString Dtime::quickHelp() const
{
  return i18n("<h1>Date & Time</h1> This control module can be used to set the system date and"
    " time. As these settings do not only affect you as a user, but rather the whole system, you"
    " can only change these settings when you start the Control Center as root. If you do not have"
    " the root password, but feel the system time should be corrected, please contact your system"
    " administrator.");
}

void Kclock::setTime(const QTime &time)
{
  this->time = time;
  repaint();
}

void Kclock::paintEvent( QPaintEvent * )
{
  if ( !isVisible() )
    return;

  QPainter paint;
  paint.begin( this );

  paint.setRenderHint(QPainter::Antialiasing);

  QPolygon pts;
  QPoint cp = rect().center();
  int d = qMin(width(),height());

  KColorScheme colorScheme( QPalette::Normal );
  QColor hands  = colorScheme.foreground(KColorScheme::NormalText).color();

  paint.setPen( hands );
  paint.setBrush( hands );
  paint.setViewport(4,4,width(),height());

  QTransform matrix;
  matrix.translate( cp.x(), cp.y() );
  matrix.scale( d/1000.0F, d/1000.0F );

  // hour hand
  float h_angle = 30*(time.hour()%12-3) + time.minute()/2;
  matrix.rotate( h_angle );
  paint.setWorldTransform( matrix );
  pts.setPoints( 4, -20,0, 0,-20, 300,0, 0,20 );
  paint.drawPolygon( pts );
  matrix.rotate( -h_angle );

  // minute hand
  float m_angle = (time.minute()-15)*6;
  matrix.rotate( m_angle );
  paint.setWorldTransform( matrix );
  pts.setPoints( 4, -10,0, 0,-10, 400,0, 0,10 );
  paint.drawPolygon( pts );
  matrix.rotate( -m_angle );

  // second hand
  float s_angle = (time.second()-15)*6;
  matrix.rotate( s_angle );
  paint.setWorldTransform( matrix );
  pts.setPoints(4,0,0,0,0,400,0,0,0);
  paint.drawPolygon( pts );
  matrix.rotate( -s_angle );

  // clock face
  for ( int i=0 ; i < 60 ; i++ ) {
    paint.setWorldTransform( matrix );
    if ( (i % 5) == 0 )
      paint.drawLine( 450,0, 500,0 ); // draw hour lines
    else  paint.drawPoint( 480,0 );   // draw second lines
    matrix.rotate( 6 );
  }

  paint.end();
}

QValidator::State KStrictIntValidator::validate( QString & input, int & d ) const
{
  if( input.isEmpty() )
    return Intermediate;

  State st = QIntValidator::validate( input, d );

  if( st == Intermediate )
    return Invalid;

  return st;
}
