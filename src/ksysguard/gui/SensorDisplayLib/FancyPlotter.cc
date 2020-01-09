/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999 - 2002 Chris Schlaeger <cs@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License version 2 or at your option version 3 as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <QtXml/qdom.h>
#include <QtGui/QImage>
#include <QtGui/QToolTip>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QFontInfo>


#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kapplication.h>
#include <kstandarddirs.h>

#include <ksgrd/SensorManager.h>
#include "StyleEngine.h"

#include "FancyPlotterSettings.h"

#include "FancyPlotter.h"

class SensorToAdd {
  public:
    QRegExp name;
    QString hostname;
    QString type;
    QList<QColor> colors;
    QString summationName;
};

class FancyPlotterLabel : public QWidget {
  public:
    FancyPlotterLabel(QWidget *parent) : QWidget(parent) {
      QBoxLayout *layout = new QHBoxLayout();
      label = new QLabel();
      layout->addWidget(label);
      value = new QLabel();
      layout->addWidget(value);
      layout->addStretch(1);
      setLayout(layout);
    }
    ~FancyPlotterLabel() {
        delete label;
        delete value;
    }
    void setLabel(const QString &name, const QColor &color, const QChar &indicatorSymbol) {
      labelName = name;
      changeLabel(color, indicatorSymbol);
    }
    void changeLabel(const QColor &color, const QChar &indicatorSymbol) {
      if ( kapp->layoutDirection() == Qt::RightToLeft )
        label->setText(QString("<qt>: ") + labelName + " <font color=\"" + color.name() + "\">" + indicatorSymbol + "</font>");
      else
        label->setText(QString("<qt><font color=\"") + color.name() + "\">" + indicatorSymbol + "</font> " + labelName + " :");
    }

    QString labelName;
    QLabel *label;
    QLabel *value;
};

FancyPlotter::FancyPlotter( QWidget* parent,
                            const QString &title,
                            SharedSettings *workSheetSettings)
  : KSGRD::SensorDisplay( parent, title, workSheetSettings )
{
  mBeams = 0;
  mNumAccountedFor = 0;
  mSettingsDialog = 0;
  mSensorReportedMax = mSensorReportedMin = 0;

  //The unicode character 0x25CF is a big filled in circle.  We would prefer to use this in the tooltip.
  //However it's maybe possible that the font used to draw the tooltip won't have it.  So we fall back to a 
  //"#" instead.
  mIndicatorSymbol = '#';
  QFontMetrics fm(QToolTip::font());
  if(fm.inFont(QChar(0x25CF)))
    mIndicatorSymbol = QChar(0x25CF);

  QBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(0);
  mPlotter = new KSignalPlotter( this );
  int axisTextWidth = fontMetrics().width(i18nc("Largest axis title", "99999 XXXX"));
  mPlotter->setMaxAxisTextWidth( axisTextWidth );
  mHeading = new QLabel(translatedTitle(), this);
  QFont headingFont;
  headingFont.setFamily("Sans Serif");
  headingFont.setWeight(QFont::Bold);
  headingFont.setPointSize(11);
  mHeading->setFont(headingFont);
  layout->addWidget(mHeading);
  layout->addWidget(mPlotter);

  /* Create a set of labels underneath the graph. */
  QBoxLayout *outerLabelLayout = new QHBoxLayout;
  outerLabelLayout->setSpacing(0);
  layout->addLayout(outerLabelLayout);
 
  /* create a spacer to fill up the space up to the start of the graph */
  outerLabelLayout->addItem(new QSpacerItem(axisTextWidth, 0, QSizePolicy::Preferred));

  mLabelLayout = new QHBoxLayout;
  outerLabelLayout->addLayout(mLabelLayout);
  QFont font;
  font.setPointSize( KSGRD::Style->fontSize() );
  mPlotter->setAxisFont( font );
  mPlotter->setAxisFontColor( Qt::black );

  mPlotter->setThinFrame(!workSheetSettings);
 
  /* All RMB clicks to the mPlotter widget will be handled by 
   * SensorDisplay::eventFilter. */
  mPlotter->installEventFilter( this );

  setPlotterWidget( mPlotter );
  connect(mPlotter, SIGNAL(axisScaleChanged()), this, SLOT(plotterAxisScaleChanged()));
  QDomElement emptyElement;
  restoreSettings(emptyElement);
}

FancyPlotter::~FancyPlotter()
{
}

void FancyPlotter::setTitle( const QString &title ) { //virtual
  KSGRD::SensorDisplay::setTitle( title );
  if(mHeading) {
    mHeading->setText(translatedTitle());
  }
}

bool FancyPlotter::eventFilter( QObject* object, QEvent* event ) {	//virtual
  if(event->type() == QEvent::ToolTip)
  {
    setTooltip();
  }
  return SensorDisplay::eventFilter(object, event);
}

void FancyPlotter::configureSettings()
{
  if(mSettingsDialog)
    return;
  mSettingsDialog = new FancyPlotterSettings( this, mSharedSettings->locked );

  mSettingsDialog->setTitle( title() );
  mSettingsDialog->setUseAutoRange( mPlotter->useAutoRange() );
  mSettingsDialog->setMinValue( mPlotter->minValue() );
  mSettingsDialog->setMaxValue( mPlotter->maxValue() );

  mSettingsDialog->setHorizontalScale( mPlotter->horizontalScale() );

  mSettingsDialog->setShowVerticalLines( mPlotter->showVerticalLines() );
  mSettingsDialog->setGridLinesColor( mPlotter->verticalLinesColor() );
  mSettingsDialog->setVerticalLinesDistance( mPlotter->verticalLinesDistance() );
  mSettingsDialog->setVerticalLinesScroll( mPlotter->verticalLinesScroll() );

  mSettingsDialog->setShowHorizontalLines( mPlotter->showHorizontalLines() );

  mSettingsDialog->setShowAxis( mPlotter->showAxis() );

  mSettingsDialog->setFontSize( mPlotter->axisFont().pointSize()  );
  mSettingsDialog->setFontColor( mPlotter->axisFontColor() );

  mSettingsDialog->setBackgroundColor( mPlotter->backgroundColor() );

  SensorModelEntry::List list;
  for ( uint i = 0; i < mBeams; ++i ) {
    SensorModelEntry entry;
    entry.setId( i );
    entry.setHostName( sensors().at( i )->hostName() );
    entry.setSensorName( sensors().at( i )->name() );
    entry.setUnit( KSGRD::SensorMgr->translateUnit( sensors().at( i )->unit() ) );
    entry.setStatus( sensors().at( i )->isOk() ? i18n( "OK" ) : i18n( "Error" ) );
    entry.setColor( mPlotter->beamColor( i ) );

    list.append( entry );
  }
  mSettingsDialog->setSensors( list );

  connect( mSettingsDialog, SIGNAL( applyClicked() ), this, SLOT( applySettings() ) );
  connect( mSettingsDialog, SIGNAL( okClicked() ), this, SLOT( applySettings() ) );
  connect( mSettingsDialog, SIGNAL( finished() ), this, SLOT( settingsFinished() ) );

  mSettingsDialog->show();
}

void FancyPlotter::settingsFinished()
{
  mSettingsDialog->delayedDestruct();
  mSettingsDialog = 0;
}

void FancyPlotter::applySettings() {
    setTitle( mSettingsDialog->title() );

    if ( mSettingsDialog->useAutoRange() )
      mPlotter->setUseAutoRange( true );
    else {
      mPlotter->setUseAutoRange( false );
      mPlotter->changeRange( mSettingsDialog->minValue(),
                            mSettingsDialog->maxValue() );
    }

    if ( mPlotter->horizontalScale() != mSettingsDialog->horizontalScale() ) {
      mPlotter->setHorizontalScale( mSettingsDialog->horizontalScale() );
    }

    mPlotter->setShowVerticalLines( mSettingsDialog->showVerticalLines() );
    mPlotter->setVerticalLinesColor( mSettingsDialog->gridLinesColor() );
    mPlotter->setVerticalLinesDistance( mSettingsDialog->verticalLinesDistance() );
    mPlotter->setVerticalLinesScroll( mSettingsDialog->verticalLinesScroll() );

    mPlotter->setShowHorizontalLines( mSettingsDialog->showHorizontalLines() );
    mPlotter->setHorizontalLinesColor( mSettingsDialog->gridLinesColor() );

    mPlotter->setShowAxis( mSettingsDialog->showAxis() );

    QFont font;
    font.setPointSize( mSettingsDialog->fontSize() );
    mPlotter->setAxisFont( font );
    mPlotter->setAxisFontColor( mSettingsDialog->fontColor() );

    mPlotter->setBackgroundColor( mSettingsDialog->backgroundColor() );

    QList<int> deletedBeams = mSettingsDialog->deleted();
    for ( int i =0; i < deletedBeams.count(); ++i) {
      removeBeam(deletedBeams[i]);
    }
    mSettingsDialog->clearDeleted(); //We have deleted them, so clear the deleted

    reorderBeams(mSettingsDialog->order());
    mSettingsDialog->resetOrder(); //We have now reordered the sensors, so reset the order

    SensorModelEntry::List list = mSettingsDialog->sensors();

    for ( int i = 0; i < sensors().count(); ++i ) {
        mPlotter->setBeamColor( i, list[ i ].color() );
        static_cast<FancyPlotterLabel *>((static_cast<QWidgetItem *>(mLabelLayout->itemAt(i)))->widget())->changeLabel(mPlotter->beamColor(i), mIndicatorSymbol);
    }
    mPlotter->update();
}
void FancyPlotter::reorderBeams(const QList<int> & orderOfBeams)
{
    //Q_ASSERT(orderOfBeams.size() == mLabelLayout.size());  Commented out because it cause compile problems in some cases??
    //Reorder the graph
    mPlotter->reorderBeams(orderOfBeams);
    //Reorder the labels underneath the graph
    QList<QLayoutItem *> labelsInOldOrder;
    while(!mLabelLayout->isEmpty())
      labelsInOldOrder.append(mLabelLayout->takeAt(0));

    for(int newIndex = 0; newIndex < orderOfBeams.count(); newIndex++) {
      int oldIndex = orderOfBeams.at(newIndex);
      mLabelLayout->addItem(labelsInOldOrder.at(oldIndex));
    }

    for ( int i = 0; i < sensors().count(); ++i ) {
      FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(i));
      for(int newIndex = 0; newIndex < orderOfBeams.count(); newIndex++) {
        int oldIndex = orderOfBeams.at(newIndex);
        if(oldIndex == sensor->beamId) {
            sensor->beamId = newIndex;
            break;
        }
      }
    }
}
void FancyPlotter::applyStyle()
{
  mPlotter->setVerticalLinesColor( KSGRD::Style->firstForegroundColor() );
  mPlotter->setHorizontalLinesColor( KSGRD::Style->secondForegroundColor() );
  mPlotter->setBackgroundColor( KSGRD::Style->backgroundColor() );
  QFont font = mPlotter->axisFont();
  font.setPointSize(KSGRD::Style->fontSize() );
  mPlotter->setAxisFont( font );
  for ( int i = 0; i < mPlotter->numBeams() &&
        (unsigned int)i < KSGRD::Style->numSensorColors(); ++i ) {
    mPlotter->setBeamColor( i, KSGRD::Style->sensorColor( i ) );
    static_cast<FancyPlotterLabel *>((static_cast<QWidgetItem *>(mLabelLayout->itemAt(i)))->widget())->changeLabel(mPlotter->beamColor(i), mIndicatorSymbol);
  }

  mPlotter->update();
}

bool FancyPlotter::addSensor( const QString &hostName, const QString &name,
                              const QString &type, const QString &title )
{
  return addSensor( hostName, name, type, title,
                    KSGRD::Style->sensorColor( mBeams ), QString(), mBeams );
}

bool FancyPlotter::addSensor( const QString &hostName, const QString &name,
                              const QString &type, const QString &title,
                              const QColor &color, const QString &regexpName,
	        	      int beamId, const QString & summationName)
{
  if ( type != "integer" && type != "float" )
    return false;


  registerSensor( new FPSensorProperties( hostName, name, type, title, color, regexpName, beamId, summationName ) );

  /* To differentiate between answers from value requests and info
   * requests we add 100 to the beam index for info requests. */
  sendRequest( hostName, name + '?', sensors().size() - 1 + 100 );

  if((int)mBeams == beamId) {
    mPlotter->addBeam( color );
    /* Add a label for this beam */
    FancyPlotterLabel *label = new FancyPlotterLabel(this);
    mLabelLayout->addWidget(label);
    if(!summationName.isEmpty()) {
      label->setLabel(summationName, mPlotter->beamColor(mBeams), mIndicatorSymbol);
    }
    ++mBeams;
  }

  return true;
}

bool FancyPlotter::removeBeam( uint beamId )
{
  if ( beamId >= mBeams ) {
    kDebug(1215) << "FancyPlotter::removeBeam: beamId out of range ("
                 << beamId << ")" << endl;
    return false;
  }

  mPlotter->removeBeam( beamId );
  --mBeams;
  QWidget *label = (static_cast<QWidgetItem *>(mLabelLayout->takeAt( beamId )))->widget();
  mLabelLayout->removeWidget(label);
  delete label;


  for ( int i = sensors().count()-1; i >= 0; --i ) {
    FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(i));
    if(sensor->beamId > (int)beamId)
      sensor->beamId--;
    else if(sensor->beamId == (int)beamId)
      removeSensor( i );
      //sensor pointer is no longer after removing the sensor
  
  }
  //adjust the scale to take into account the removed sensor
  plotterAxisScaleChanged();

  return true;
}

void FancyPlotter::setTooltip()
{
  QString tooltip = "<qt><p style='white-space:pre'>";

  QString description;
  QString lastValue;
  bool neednewline = false;
  int beamId = -1;
  //Note that the number of beams can be less than the number of sensors, since some sensors
  //get added together for a beam.
  //For the tooltip, we show all the sensors
  for ( int i = 0; i < sensors().count(); ++i ) {
    FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(i));
    description = sensor->description();
    if(description.isEmpty())
      description = sensor->name();

    if(sensor->isOk()) {
      lastValue = KGlobal::locale()->formatNumber( sensor->lastValue, (sensor->isInteger)?0:-1 );
      if (mUnit == "%")
        lastValue += "%";
      else if( mUnit != "" )
        lastValue += " " + mUnit;
    } else {
      lastValue = i18n("Error");
    }
    if(beamId != sensor->beamId && !sensor->summationName.isEmpty()) {
      tooltip += i18nc("%1 is what is being shown statistics for, like 'Memory', 'Swap', etc.", "<p><b>%1:</b><br>", sensor->summationName);
      neednewline = false;
    }
    beamId = sensor->beamId;

    if(sensor->isLocalhost()) {
      tooltip += QString( "%1%2 %3 (%4)" ).arg( neednewline  ? "<br>" : "")
            .arg("<font color=\"" + mPlotter->beamColor( beamId ).name() + "\">"+mIndicatorSymbol+"</font>")
            .arg( i18n(description.toLatin1()) )
            .arg( lastValue );

    } else {
      tooltip += QString( "%1%2 %3:%4 (%5)" ).arg( neednewline ? "<br>" : "" )
                 .arg("<font color=\"" + mPlotter->beamColor( beamId ).name() + "\">"+mIndicatorSymbol+"</font>")
                 .arg( sensor->hostName() )
                 .arg( i18n(description.toLatin1()) )
	         .arg( lastValue );
    }
    neednewline = true;
  }
//  tooltip += "</td></tr></table>";
  mPlotter->setToolTip( tooltip );
}

void FancyPlotter::timerTick( ) //virtual
{
  if(!mSampleBuf.isEmpty() && mBeams != 0) {  
    if((uint)mSampleBuf.count() > mBeams) {
      mSampleBuf.clear();
      return; //ignore invalid results - can happen if a sensor is deleted
    }
    while((uint)mSampleBuf.count() < mBeams)
      mSampleBuf.append(mPlotter->lastValue(mSampleBuf.count())); //we might have sensors missing so set their values to the previously known value
    mPlotter->addSample( mSampleBuf );
    if(QToolTip::isVisible()) {
      setTooltip();
    }
    if(isVisible()) {
      QString lastValue;
      int beamId = -1;
      for ( int i = 0; i < sensors().size(); ++i ) {
        FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(i));
        if(sensor->beamId == beamId)
          continue;
        beamId = sensor->beamId;
        if(sensor->isOk() && mPlotter->numBeams() > beamId) {
          int precision = (sensor->isInteger && mPlotter->scaleDownBy() == 1)?0:-1;
          lastValue = mPlotter->lastValueAsString(beamId, precision);
          if(sensor->maxValue != 0 && mUnit != "%")
            lastValue = i18nc("%1 and %2 are sensor's last and maximum value", "%1 of %2", lastValue, mPlotter->valueAsString(sensor->maxValue, precision) );
        } else {
          lastValue = i18n("Error");
        }

        static_cast<FancyPlotterLabel *>((static_cast<QWidgetItem *>(mLabelLayout->itemAt(beamId)))->widget())->value->setText(lastValue);
      }
    }

  }
  mSampleBuf.clear();

  SensorDisplay::timerTick();
}
void FancyPlotter::plotterAxisScaleChanged()
{
    //Prevent this being called recursively
    disconnect(mPlotter, SIGNAL(axisScaleChanged()), this, SLOT(plotterAxisScaleChanged()));
    QString unit = mUnit.toUpper();
    double value = mPlotter->maxValue();
    if(unit == "KB" || unit  == "KIB") {
        if(value >= 1024*1024*1024*0.7) {  //If it's over 0.7TiB, then set the scale to terabytes
            mPlotter->setScaleDownBy(1024*1024*1024);
            unit = "TiB";
        } else if(value >= 1024*1024*0.7) {  //If it's over 0.7GiB, then set the scale to gigabytes
            mPlotter->setScaleDownBy(1024*1024);
            unit = "GiB";
        } else if(value > 1024) {
            mPlotter->setScaleDownBy(1024);
            unit = "MiB";
        } else {
            mPlotter->setScaleDownBy(1);
            unit = "KiB";
        }
    } else if(unit == "KB/S" || unit == "KIB/S") {
        if(value >= 1024*1024*1024*0.7) {  //If it's over 0.7TiB, then set the scale to terabytes
            mPlotter->setScaleDownBy(1024*1024*1024);
            unit = "TiB/s";
        } else if(value >= 1024*1024*0.7) {  //If it's over 0.7GiB, then set the scale to gigabytes
            mPlotter->setScaleDownBy(1024*1024);
            unit = "GiB/s";
        } else if(value > 1024) {
            mPlotter->setScaleDownBy(1024);
            unit = "MiB/s";
        } else {
            mPlotter->setScaleDownBy(1);
            unit = "KiB/s";
        }
    } else {
        //todo - also handle s for seconds - for uptime etc
        mPlotter->setScaleDownBy(1);
        unit = mUnit;
    }
    unit = KSGRD::SensorMgr->translateUnit( unit );
    mPlotter->setUnit(unit);
    //reconnect
    connect(mPlotter, SIGNAL(axisScaleChanged()), this, SLOT(plotterAxisScaleChanged()));
}
void FancyPlotter::answerReceived( int id, const QList<QByteArray> &answerlist )
{
  QByteArray answer;

  if(!answerlist.isEmpty()) answer = answerlist[0];
  if ( (uint)id < 100 ) {
    //Make sure that we put the answer in the correct place.  It's index in the list should be equal to the sensor index.  This in turn will contain the beamId
    
    FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(id));
    int beamId = sensor->beamId;
    double value = answer.toDouble();
    while(beamId > mSampleBuf.count())
      mSampleBuf.append(0); //we might have sensors missing so set their values to zero

    if(beamId == mSampleBuf.count()) {
      mSampleBuf.append( value );
    } else {
      mSampleBuf[beamId] += value; //If we get two answers for the same beamid, we should add them together.  That's how the summation works
    }
    sensor->lastValue = value;
    /* We received something, so the sensor is probably ok. */
    sensorError( id, false );
  } else if ( id >= 100 && id < 200 ) {
    KSGRD::SensorFloatInfo info( answer );
    mUnit = info.unit();
    mSensorReportedMax = qMax(mSensorReportedMax, info.max());
    mSensorReportedMin = qMin(mSensorReportedMin, info.min());
    if(mSensorReportedMax == 0 && mSensorReportedMin)
      mPlotter->setUseAutoRange(true); // If any of the sensors are using autorange, then the whole graph must use auto range

    if ( !mPlotter->useAutoRange()) {
      mPlotter->changeRange( mSensorReportedMin, mSensorReportedMax );
      plotterAxisScaleChanged(); //Change the scale now since we know what it is. If instead we are using auto range, then plotterAxisScaleChanged will be called when the first bits of data come in
    } else
        mPlotter->setUnit( KSGRD::SensorMgr->translateUnit( mUnit ) );
 
    FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(id - 100));
    sensor->maxValue = info.max();
    sensor->setUnit( info.unit() );
    sensor->setDescription( info.name() );

    QString summationName = sensor->summationName;
    int beamId = sensor->beamId;

    if(summationName.isEmpty())
      static_cast<FancyPlotterLabel *>((static_cast<QWidgetItem *>(mLabelLayout->itemAt(beamId)))->widget())->setLabel(info.name(), mPlotter->beamColor(id-100), mIndicatorSymbol);

  } else if( id == 200) {
    /* FIXME This doesn't check the host!  */
    if(!mSensorsToAdd.isEmpty())  {
      foreach(SensorToAdd *sensor, mSensorsToAdd) {
        int beamId = mBeams;  //Assign the next sensor to the next available beamId
        for ( int i = 0; i < answerlist.count(); ++i ) {
          if ( answerlist[ i ].isEmpty() )
            continue;
          QString sensorName = QString::fromUtf8(answerlist[ i ].split('\t')[0]);
          if(sensor->name.exactMatch(sensorName)) {
            if(sensor->summationName.isEmpty())
              beamId = mBeams; //If summationName is not empty then reuse the previous beamId.  In this way we can have multiple sensors with the same beamId, which can then be summed together
            QColor color;
            if(!sensor->colors.isEmpty() )
                color = sensor->colors.takeFirst();
            else if(KSGRD::Style->numSensorColors() != 0)
                color = KSGRD::Style->sensorColor( beamId % KSGRD::Style->numSensorColors());
            addSensor( sensor->hostname, sensorName,
                   (sensor->type.isEmpty()) ? "float" : sensor->type
                    , "", color, sensor->name.pattern(), beamId, sensor->summationName);
          }
        }
      }
      foreach(SensorToAdd *sensor, mSensorsToAdd) {
        delete sensor;
      }
      mSensorsToAdd.clear();
    }
  }
}

bool FancyPlotter::restoreSettings( QDomElement &element )
{
  /* autoRange was added after KDE 2.x and was brokenly emulated by
   * min == 0.0 and max == 0.0. Since we have to be able to read old
   * files as well we have to emulate the old behaviour as well. */
  double min = element.attribute( "min", "0.0" ).toDouble();
  double max = element.attribute( "max", "0.0" ).toDouble();
  if ( element.attribute( "autoRange", min == 0.0 && max == 0.0 ? "1" : "0" ).toInt() )
    mPlotter->setUseAutoRange( true );
  else {
    mPlotter->setUseAutoRange( false );
    mPlotter->changeRange( element.attribute( "min" ).toDouble(),
                           element.attribute( "max" ).toDouble() );
  }
  // Do not restore the color settings from a previous version
  int version = element.attribute("version", "0").toInt();

  mPlotter->setShowVerticalLines( element.attribute( "vLines", "0" ).toUInt() );
  QColor vcolor = restoreColor( element, "vColor", mPlotter->verticalLinesColor() );
  mPlotter->setVerticalLinesColor( vcolor );
  mPlotter->setVerticalLinesDistance( element.attribute( "vDistance", "30" ).toUInt() );
  mPlotter->setVerticalLinesScroll( element.attribute( "vScroll", "0" ).toUInt() );
  mPlotter->setHorizontalScale( element.attribute( "hScale", "6" ).toUInt() );
  
  mPlotter->setShowHorizontalLines( element.attribute( "hLines", "1" ).toUInt() );
  mPlotter->setHorizontalLinesColor( restoreColor( element, "hColor",
                                     mPlotter->horizontalLinesColor() ) );
  
  QString filename = element.attribute( "svgBackground");
  if (!filename.isEmpty() && filename[0] == '/') {
    KStandardDirs* kstd = KGlobal::dirs();
    filename = kstd->findResource( "data", "ksysguard/" + filename);
  }
  mPlotter->setSvgBackground( filename );
  if(version >= 1) {
    mPlotter->setShowAxis( element.attribute( "labels", "1" ).toUInt() );
    uint fontsize = element.attribute( "fontSize", "0").toUInt();
    if(fontsize == 0) fontsize =  KSGRD::Style->fontSize();
    QFont font;
    font.setPointSize( fontsize );
  
    mPlotter->setAxisFont( font );
  
    mPlotter->setAxisFontColor( restoreColor( element, "fontColor", Qt::black ) );  //make the default to be the same as the vertical line color
    mPlotter->setBackgroundColor( restoreColor( element, "bColor",
                                   KSGRD::Style->backgroundColor() ) );
  }
  QDomNodeList dnList = element.elementsByTagName( "beam" );
  for ( int i = 0; i < dnList.count(); ++i ) {
    QDomElement el = dnList.item( i ).toElement();
    if(el.hasAttribute("regexpSensorName")) {
      SensorToAdd *sensor = new SensorToAdd();
      sensor->name = QRegExp(el.attribute("regexpSensorName"));
      sensor->hostname = el.attribute( "hostName" );
      sensor->type = el.attribute( "sensorType" );
      sensor->summationName = el.attribute("summationName");
      QStringList colors = el.attribute("color").split(',');
      bool ok;
      foreach(const QString &color, colors) {
        int c = color.toUInt( &ok, 0 );
        if(ok) {
          QColor col( (c & 0xff0000) >> 16, (c & 0xff00) >> 8, (c & 0xff), (c & 0xff000000) >> 24);
          if(col.isValid()) {
            if(col.alpha() == 0) col.setAlpha(255);
            sensor->colors << col;
          }
          else
           sensor->colors << KSGRD::Style->sensorColor( i );
        }
        else
          sensor->colors << KSGRD::Style->sensorColor( i );
      }
      mSensorsToAdd.append(sensor);
      sendRequest( sensor->hostname, "monitors", 200 );
    } else
      addSensor( el.attribute( "hostName" ), el.attribute( "sensorName" ),
               ( el.attribute( "sensorType" ).isEmpty() ? "float" :
               el.attribute( "sensorType" ) ), "", restoreColor( el, "color",
               KSGRD::Style->sensorColor( i ) ), QString(), mBeams, el.attribute("summationName") );
  }

  SensorDisplay::restoreSettings( element );

  return true;
}

bool FancyPlotter::saveSettings( QDomDocument &doc, QDomElement &element)
{
  element.setAttribute( "min", mPlotter->minValue() );
  element.setAttribute( "max", mPlotter->maxValue() );
  element.setAttribute( "autoRange", mPlotter->useAutoRange() );
  element.setAttribute( "vLines", mPlotter->showVerticalLines() );
  saveColor( element, "vColor", mPlotter->verticalLinesColor() );
  element.setAttribute( "vDistance", mPlotter->verticalLinesDistance() );
  element.setAttribute( "vScroll", mPlotter->verticalLinesScroll() );

  element.setAttribute( "hScale", mPlotter->horizontalScale() );

  element.setAttribute( "hLines", mPlotter->showHorizontalLines() );
  saveColor( element, "hColor", mPlotter->horizontalLinesColor() );

  element.setAttribute( "svgBackground", mPlotter->svgBackground() );

  element.setAttribute( "version", 1 );
  element.setAttribute( "labels", mPlotter->showAxis() );
  saveColor ( element, "fontColor", mPlotter->axisFontColor());

  saveColor( element, "bColor", mPlotter->backgroundColor() );

  QHash<QString,QDomElement> hash;
  int beamId = -1;
  for ( int i = 0; i < sensors().size(); ++i ) {
    FPSensorProperties *sensor = static_cast<FPSensorProperties *>(sensors().at(i));
    if(sensor->beamId == beamId)
      continue;
    beamId = sensor->beamId;

    QString regExpName = sensor->regExpName();
    if(!regExpName.isEmpty() && hash.contains( regExpName )) {
      QDomElement oldBeam = hash.value(regExpName);
      saveColorAppend( oldBeam, "color", mPlotter->beamColor( beamId ) );
    } else {
      QDomElement beam = doc.createElement( "beam" );
      element.appendChild( beam );
      beam.setAttribute( "hostName", sensor->hostName() );
      if(regExpName.isEmpty())
        beam.setAttribute( "sensorName", sensor->name() );
      else {
          beam.setAttribute( "regexpSensorName", sensor->regExpName() );
          hash[regExpName] = beam;
      }
      if(!sensor->summationName.isEmpty())
        beam.setAttribute( "summationName", sensor->summationName);
      beam.setAttribute( "sensorType", sensor->type() );
      saveColor( beam, "color", mPlotter->beamColor( beamId ) );
    }
  }
  SensorDisplay::saveSettings( doc, element );

  return true;
}

bool FancyPlotter::hasSettingsDialog() const
{
  return true;
}

FPSensorProperties::FPSensorProperties()
{
}

FPSensorProperties::FPSensorProperties( const QString &hostName,
                                        const QString &name,
                                        const QString &type,
                                        const QString &description,
                                        const QColor &color,
                                        const QString &regexpName,
                                        int beamId_,
                                        const QString &summationName_ )
  : KSGRD::SensorProperties( hostName, name, type, description),
    mColor( color )
{
  setRegExpName(regexpName);
  beamId = beamId_;
  summationName = summationName_;
  maxValue = 0;
  lastValue = 0;
  isInteger = (type == "integer");
}

FPSensorProperties::~FPSensorProperties()
{
}

void FPSensorProperties::setColor( const QColor &color )
{
  mColor = color;
}

QColor FPSensorProperties::color() const
{
  return mColor;
}


#include "FancyPlotter.moc"
