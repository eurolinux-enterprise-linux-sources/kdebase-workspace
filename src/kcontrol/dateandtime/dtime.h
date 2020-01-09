/*
 *  dtime.h
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

#ifndef dtime_included
#define dtime_included

#include <QDateTime>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QTimer>
#include <QValidator>
#include <QWidget>
#include <QCheckBox>
//Added by qt3to4:
#include <QPaintEvent>

#include <kdatepicker.h>
#include <knuminput.h>

class Kclock;

class HMSTimeWidget : public KIntSpinBox
{
  Q_OBJECT
 public:
  HMSTimeWidget(QWidget *parent=0);
 protected:
  QString mapValueToText(int);
};

class Dtime : public QWidget
{
  Q_OBJECT
 public:
  Dtime( QWidget *parent=0 );

  void	save( QStringList& helperargs );
  void processHelperErrors( int code );
  void	load();

  QString quickHelp() const;

Q_SIGNALS:
  void	timeChanged(bool);

 private Q_SLOTS:
  void	configChanged();
  void	serverTimeCheck();
  void	timeout();
  void	set_time();
  void	changeDate(const QDate&);

private:
  void	findNTPutility();
  QString	ntpUtility;

  QWidget*	privateLayoutWidget;
  QCheckBox	*setDateTimeAuto;
  QComboBox	*timeServerList;

  KDatePicker	*cal;
  QComboBox	*month;
  QSpinBox	*year;

  HMSTimeWidget	*hour;
  HMSTimeWidget	*minute;
  HMSTimeWidget	*second;

  Kclock	*kclock;

  QTime		time;
  QDate		date;
  QTimer	internalTimer;

  QString       timeServer;
  int		BufI;
  bool		refresh;
  bool		ontimeout;
};

class Kclock : public QWidget
{
  Q_OBJECT

public:
  Kclock( QWidget *parent=0 )
    : QWidget(parent) {}

  void setTime(const QTime&);

protected:
  virtual void	paintEvent( QPaintEvent *event );


private:
  QTime		time;
};

class KStrictIntValidator : public QIntValidator
{
public:
  KStrictIntValidator(int bottom, int top, QWidget * parent)
    : QIntValidator(bottom, top, parent) {}

  QValidator::State validate( QString & input, int & d ) const;
};

#endif // dtime_included
