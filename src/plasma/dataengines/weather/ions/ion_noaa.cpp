/***************************************************************************
 *   Copyright (C) 2007-2009 by Shawn Starr <shawn.starr@rogers.com>       *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

/* Ion for NOAA's National Weather Service XML data */

#include "ion_noaa.h"

class NOAAIon::Private : public QObject
{
public:
    struct XMLMapInfo {
        QString stateName;
        QString stationName;
        QString stationID;
        QString XMLurl;
    };

public:
    // Key dicts
    QHash<QString, NOAAIon::Private::XMLMapInfo> m_places;

    // Weather information
    QHash<QString, WeatherData> m_weatherData;

    // Store KIO jobs
    QMap<KJob *, QXmlStreamReader*> m_jobXml;
    QMap<KJob *, QString> m_jobList;
    QXmlStreamReader m_xmlSetup;

    QDateTime m_dateFormat;
    bool emitWhenSetup;

    Private() : emitWhenSetup(false) {
    }

};

QMap<QString, IonInterface::WindDirections> NOAAIon::setupWindIconMappings(void)
{
    QMap<QString, WindDirections> windDir;
    windDir["north"] = N;
    windDir["northeast"] = NE;
    windDir["south"] = S;
    windDir["southwest"] = SW;
    windDir["east"] = E;
    windDir["southeast"] = SE;
    windDir["west"] = W;
    windDir["northwest"] = NW;
    windDir["calm"] = VR;
    return windDir;
}

QMap<QString, IonInterface::ConditionIcons> NOAAIon::setupConditionIconMappings(void)
{

    QMap<QString, ConditionIcons> conditionList;
    return conditionList;
}

QMap<QString, IonInterface::ConditionIcons> const& NOAAIon::conditionIcons(void)
{
    static QMap<QString, ConditionIcons> const condval = setupConditionIconMappings();
    return condval;
}

QMap<QString, IonInterface::WindDirections> const& NOAAIon::windIcons(void)
{
    static QMap<QString, WindDirections> const wval = setupWindIconMappings();
    return wval;
}

// ctor, dtor
NOAAIon::NOAAIon(QObject *parent, const QVariantList &args)
        : IonInterface(parent, args), d(new Private())
{
    Q_UNUSED(args)
}

void NOAAIon::reset()
{
    delete d;
    d = new Private();
    d->emitWhenSetup = true;
    setInitialized(false);
    getXMLSetup();
}

NOAAIon::~NOAAIon()
{
    // Destroy dptr
    delete d;
}

// Get the master list of locations to be parsed
void NOAAIon::init()
{
    // Get the real city XML URL so we can parse this
    getXMLSetup();
}

QStringList NOAAIon::validate(const QString& source) const
{
    QStringList placeList;
    QString station;
    QString sourceNormalized = source.toUpper();

    QHash<QString, NOAAIon::Private::XMLMapInfo>::const_iterator it = d->m_places.constBegin();
    // If the source name might look like a station ID, check these too and return the name
    bool checkState = source.count() == 2;

    while (it != d->m_places.constEnd()) {
        if (checkState) {
            if (it.value().stateName == source) {
                placeList.append(QString("place|").append(it.key()));
            }
        } else if (it.key().toUpper().contains(sourceNormalized)) {
            placeList.append(QString("place|").append(it.key()));
        } else if (it.value().stationID == sourceNormalized) {
            station = QString("place|").append(it.key());
        }

        ++it;
    }

    placeList.sort();
    if (!station.isEmpty()) {
        placeList.prepend(station);
    }

    return placeList;
}

bool NOAAIon::updateIonSource(const QString& source)
{
    // We expect the applet to send the source in the following tokenization:
    // ionname:validate:place_name - Triggers validation of place
    // ionname:weather:place_name - Triggers receiving weather of place

    QStringList sourceAction = source.split('|');

    // Guard: if the size of array is not 2 then we have bad data, return an error
    if (sourceAction.size() < 2) {
        setData(source, "validate", "noaa|malformed");
        return true;
    }

    if (sourceAction[1] == "validate" && sourceAction.size() > 2) {
        QStringList result = validate(sourceAction[2]);

        if (result.size() == 1) {
            setData(source, "validate", QString("noaa|valid|single|").append(result.join("|")));
            return true;
        } else if (result.size() > 1) {
            setData(source, "validate", QString("noaa|valid|multiple|").append(result.join("|")));
            return true;
        } else if (result.size() == 0) {
            setData(source, "validate", QString("noaa|invalid|single|").append(sourceAction[2]));
            return true;
        }

    } else if (sourceAction[1] == "weather" && sourceAction.size() > 2) {
        getXMLData(source);
        return true;
    } else {
        setData(source, "validate", "noaa|malformed");
        return true;
    }

    return false;
}

// Parses city list and gets the correct city based on ID number
void NOAAIon::getXMLSetup()
{
    KIO::TransferJob *job = KIO::get(KUrl("http://www.weather.gov/data/current_obs/index.xml"), KIO::NoReload, KIO::HideProgressInfo);

    if (job) {
        connect(job, SIGNAL(data(KIO::Job *, const QByteArray &)), this,
                SLOT(setup_slotDataArrived(KIO::Job *, const QByteArray &)));
        connect(job, SIGNAL(result(KJob *)), this, SLOT(setup_slotJobFinished(KJob *)));
    } else {
        kDebug() << "Could not create place name list transfer job";
    }
}

// Gets specific city XML data
void NOAAIon::getXMLData(const QString& source)
{
    KUrl url;

    QString dataKey = source;
    dataKey.remove("noaa|weather|");
    url = d->m_places[dataKey].XMLurl;

    // If this is empty we have no valid data, send out an error and abort.
    if (url.url().isEmpty()) {
        setData(source, "validate", QString("noaa|malformed"));
        return;
    }

    KIO::TransferJob * const m_job = KIO::get(url.url(), KIO::Reload, KIO::HideProgressInfo);
    d->m_jobXml.insert(m_job, new QXmlStreamReader);
    d->m_jobList.insert(m_job, source);

    if (m_job) {
        connect(m_job, SIGNAL(data(KIO::Job *, const QByteArray &)), this,
             SLOT(slotDataArrived(KIO::Job *, const QByteArray &)));
        connect(m_job, SIGNAL(result(KJob *)), this, SLOT(slotJobFinished(KJob *)));
    }
}

void NOAAIon::setup_slotDataArrived(KIO::Job *job, const QByteArray &data)
{
    Q_UNUSED(job)

    if (data.isEmpty()) {
        return;
    }

    // Send to xml.
    d->m_xmlSetup.addData(data);
}

void NOAAIon::slotDataArrived(KIO::Job *job, const QByteArray &data)
{
    if (data.isEmpty() || !d->m_jobXml.contains(job)) {
        return;
    }

    // Send to xml.
    d->m_jobXml[job]->addData(data);
}

void NOAAIon::slotJobFinished(KJob *job)
{
    // Dual use method, if we're fetching location data to parse we need to do this first
    removeAllData(d->m_jobList[job]);
    QXmlStreamReader *reader = d->m_jobXml.value(job);
    if (reader) {
        readXMLData(d->m_jobList[job], *reader);
    }

    d->m_jobList.remove(job);
    d->m_jobXml.remove(job);
    delete reader;
}

void NOAAIon::setup_slotJobFinished(KJob *job)
{
    Q_UNUSED(job)
    const bool success = readXMLSetup();
    setInitialized(success);
    if (d->emitWhenSetup) {
        d->emitWhenSetup = false;
        emit(resetCompleted(this, success));
    }
}

void NOAAIon::parseStationID()
{
    QString state;
    QString stationName;
    QString stationID;
    QString xmlurl;

    while (!d->m_xmlSetup.atEnd()) {
        d->m_xmlSetup.readNext();

        if (d->m_xmlSetup.isEndElement() && d->m_xmlSetup.name() == "station") {
            if (!xmlurl.isEmpty()) {
                NOAAIon::Private::XMLMapInfo info;
                info.stateName = state;
                info.stationName = stationName;
                info.stationID = stationID;
                info.XMLurl = xmlurl;

                QString tmp = stationName + ", " + state; // Build the key name.
                d->m_places[tmp] = info;
            }
            break;
        }

        if (d->m_xmlSetup.isStartElement()) {
            if (d->m_xmlSetup.name() == "station_id") {
                stationID = d->m_xmlSetup.readElementText();
            } else if (d->m_xmlSetup.name() == "state") {
                state = d->m_xmlSetup.readElementText();
            } else if (d->m_xmlSetup.name() == "station_name") {
                stationName = d->m_xmlSetup.readElementText();
            } else if (d->m_xmlSetup.name() == "xml_url") {
                xmlurl = d->m_xmlSetup.readElementText().replace("http://", "http://www.");
            } else {
                parseUnknownElement(d->m_xmlSetup);
            }
        }
    }
}

void NOAAIon::parseStationList()
{
    while (!d->m_xmlSetup.atEnd()) {
        d->m_xmlSetup.readNext();

        if (d->m_xmlSetup.isEndElement()) {
            break;
        }

        if (d->m_xmlSetup.isStartElement()) {
            if (d->m_xmlSetup.name() == "station") {
                parseStationID();
            } else {
                parseUnknownElement(d->m_xmlSetup);
            }
        }
    }
}

// Parse the city list and store into a QMap
bool NOAAIon::readXMLSetup()
{
    bool success = false;
    while (!d->m_xmlSetup.atEnd()) {
        d->m_xmlSetup.readNext();

        if (d->m_xmlSetup.isStartElement()) {
            if (d->m_xmlSetup.name() == "wx_station_index") {
                parseStationList();
                success = true;
            }
        }
    }
    return (!d->m_xmlSetup.error() && success);
}

WeatherData NOAAIon::parseWeatherSite(WeatherData& data, QXmlStreamReader& xml)
{
    data.temperature_C = "N/A";
    data.temperature_F = "N/A";
    data.dewpoint_C = "N/A";
    data.dewpoint_F = "N/A";
    data.weather = "N/A";
    data.stationID = "N/A";
    data.pressure = "N/A";
    data.visibility = "N/A";
    data.humidity = "N/A";
    data.windSpeed = "N/A";
    data.windGust = "N/A";
    data.windchill_F = "N/A";
    data.windchill_C = "N/A";
    data.heatindex_F = "N/A";
    data.heatindex_C = "N/A";

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            if (xml.name() == "location") {
                data.locationName = xml.readElementText();
            } else if (xml.name() == "station_id") {
                data.stationID = xml.readElementText();
            } else if (xml.name() == "latitude") {
                data.stationLat = xml.readElementText();
            } else if (xml.name() == "longitude") {
                data.stationLon = xml.readElementText();
            } else if (xml.name() == "observation_time") {
                data.observationTime = xml.readElementText();
                QStringList tmpDateStr = data.observationTime.split(' ');
                data.observationTime = QString("%1 %2").arg(tmpDateStr[5]).arg(tmpDateStr[6]);
                d->m_dateFormat = QDateTime::fromString(data.observationTime, "h:mm ap");
                data.iconPeriodHour = d->m_dateFormat.toString("HH");
                data.iconPeriodAP = d->m_dateFormat.toString("ap");

            } else if (xml.name() == "weather") {
                data.weather = xml.readElementText();
                // Pick which icon set depending on period of day
            } else if (xml.name() == "temp_f") {
                data.temperature_F = xml.readElementText();
            } else if (xml.name() == "temp_c") {
                data.temperature_C = xml.readElementText();
            } else if (xml.name() == "relative_humidity") {
                data.humidity = xml.readElementText();
            } else if (xml.name() == "wind_dir") {
                data.windDirection = xml.readElementText();
            } else if (xml.name() == "wind_mph") {
                data.windSpeed = xml.readElementText();
            } else if (xml.name() == "wind_gust_mph") {
                data.windGust = xml.readElementText();
            } else if (xml.name() == "pressure_in") {
                data.pressure = xml.readElementText();
            } else if (xml.name() == "dewpoint_f") {
                data.dewpoint_F = xml.readElementText();
            } else if (xml.name() == "dewpoint_c") {
                data.dewpoint_C = xml.readElementText();
            } else if (xml.name() == "heat_index_f") {
                data.heatindex_F = xml.readElementText();
            } else if (xml.name() == "heat_index_c") {
                data.heatindex_C = xml.readElementText();
            } else if (xml.name() == "windchill_f") {
                data.windchill_F = xml.readElementText();
            } else if (xml.name() == "windchill_c") {
                data.windchill_C = xml.readElementText();
            } else if (xml.name() == "visibility_mi") {
                data.visibility = xml.readElementText();
            } else {
                parseUnknownElement(xml);
            }
        }
    }
    return data;
}

// Parse Weather data main loop, from here we have to decend into each tag pair
bool NOAAIon::readXMLData(const QString& source, QXmlStreamReader& xml)
{
    WeatherData data;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isEndElement()) {
            break;
        }

        if (xml.isStartElement()) {
            if (xml.name() == "current_observation") {
                data = parseWeatherSite(data, xml);
            } else {
                parseUnknownElement(xml);
            }
        }
    }

    d->m_weatherData[source] = data;
    updateWeather(source);
    return !xml.error();
}

// handle when no XML tag is found
void NOAAIon::parseUnknownElement(QXmlStreamReader& xml)
{

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isEndElement()) {
            break;
        }

        if (xml.isStartElement()) {
            parseUnknownElement(xml);
        }
    }
}

void NOAAIon::updateWeather(const QString& source)
{
    QMap<QString, QString> dataFields;
    Plasma::DataEngine::Data data;

    data.insert("Country", country(source));
    data.insert("Place", place(source));
    data.insert("Station", station(source));

    data.insert("Latitude", latitude(source));
    data.insert("Longitude", longitude(source));

    // Real weather - Current conditions
    data.insert("Observation Period", observationTime(source));
    data.insert("Current Conditions", condition(source));
    kDebug() << "i18n condition string: " << qPrintable(condition(source));
// FIXME: We'll need major fuzzy logic, this isn't pretty: http://www.weather.gov/xml/current_obs/weather.php
    //QMap<QString, ConditionIcons> conditionList;
    //conditionList = conditionIcons();

    /*
        if ((periodHour(source) >= 0 && periodHour(source) < 6) || (periodHour(source) >= 18)) {
            // Night
            // - Fill in condition fuzzy logic
        } else {
            // Day
            // - Fill in condition fuzzy logic
        }
    */
    data.insert("Condition Icon", "weather-none-available");

    dataFields = temperature(source);
    data.insert("Temperature", dataFields["temperature"]);

    if (dataFields["temperature"] != "N/A") {
        data.insert("Temperature Unit", dataFields["temperatureUnit"]);
    }

    // Do we have a comfort temperature? if so display it
    if (dataFields["comfortTemperature"] != "N/A") {
        if (d->m_weatherData[source].windchill_F != "NA") {
            data.insert("Windchill", QString("%1").arg(dataFields["comfortTemperature"]));
            data.insert("Humidex", "N/A");
        }
        if (d->m_weatherData[source].heatindex_F != "NA" && d->m_weatherData[source].temperature_F.toInt() != d->m_weatherData[source].heatindex_F.toInt()) {
            data.insert("Humidex", QString("%1").arg(dataFields["comfortTemperature"]));
            data.insert("Windchill", "N/A");
        }
    } else {
        data.insert("Windchill", "N/A");
        data.insert("Humidex", "N/A");
    }

    data.insert("Dewpoint", dewpoint(source));
    if (dewpoint(source) != "N/A") {
        data.insert("Dewpoint Unit", dataFields["temperatureUnit"]);
    }

    dataFields = pressure(source);
    data.insert("Pressure", dataFields["pressure"]);

    if (dataFields["pressure"] != "N/A") {
        data.insert("Pressure Unit", dataFields["pressureUnit"]);
    }

    dataFields = visibility(source);
    data.insert("Visibility", dataFields["visibility"]);

    if (dataFields["visibility"] != "N/A") {
        data.insert("Visibility Unit", dataFields["visibilityUnit"]);
    }

    if (humidity(source) != "N/A") {
        data.insert("Humidity", i18nc("Humidity in percent", "%1%", humidity(source))); // FIXME: Turn '%' into a unit field
    }

    // Set number of forecasts per day/night supported, none for this ion right now
    data.insert(QString("Total Weather Days"), 0);

    dataFields = wind(source);
    data.insert("Wind Speed", dataFields["windSpeed"]);

    if (dataFields["windSpeed"] != "Calm") {
        data.insert("Wind Speed Unit", dataFields["windUnit"]);
    }

    data.insert("Wind Gust", dataFields["windGust"]);
    data.insert("Wind Gust Unit", dataFields["windGustUnit"]);
    data.insert("Wind Direction", getWindDirectionIcon(windIcons(), dataFields["windDirection"].toLower()));
    data.insert("Credit", i18n("Data provided by NOAA National Weather Service"));

    setData(source, data);
}

QString NOAAIon::country(const QString& source)
{
    Q_UNUSED(source);
    return QString("USA");
}

QString NOAAIon::place(const QString& source)
{
    return d->m_weatherData[source].locationName;
}

QString NOAAIon::station(const QString& source)
{
    return d->m_weatherData[source].stationID;
}

QString NOAAIon::latitude(const QString& source)
{
    return d->m_weatherData[source].stationLat;
}

QString NOAAIon::longitude(const QString& source)
{
    return d->m_weatherData[source].stationLon;
}

QString NOAAIon::observationTime(const QString& source)
{
    return d->m_weatherData[source].observationTime;
}

/*
bool NOAAIon::night(const QString& source)
{
    if (d->m_weatherData[source].iconPeriodAP == "pm") {
        return true;
    }
    return false;
}
*/

int NOAAIon::periodHour(const QString& source)
{
    return d->m_weatherData[source].iconPeriodHour.toInt();
}

QString NOAAIon::condition(const QString& source)
{
    if (d->m_weatherData[source].weather.isEmpty() || d->m_weatherData[source].weather == "NA") {
        d->m_weatherData[source].weather = "N/A";
    }
    return i18nc("weather condition", d->m_weatherData[source].weather.toUtf8());
}

QString NOAAIon::dewpoint(const QString& source)
{
    return d->m_weatherData[source].dewpoint_F;
}

QString NOAAIon::humidity(const QString& source)
{
    if (d->m_weatherData[source].humidity == "NA") {
        return QString("N/A");
    } else {
        return (d->m_weatherData[source].humidity);
    }
}

QMap<QString, QString> NOAAIon::visibility(const QString& source)
{
    QMap<QString, QString> visibilityInfo;
    if (d->m_weatherData[source].visibility.isEmpty()) {
        visibilityInfo.insert("visibility", QString("N/A"));
        return visibilityInfo;
    }
    if (d->m_weatherData[source].visibility == "NA") {
        visibilityInfo.insert("visibility", QString("N/A"));
        visibilityInfo.insert("visibilityUnit", QString::number(WeatherUtils::NoUnit));
    } else {
        visibilityInfo.insert("visibility", d->m_weatherData[source].visibility);
        visibilityInfo.insert("visibilityUnit", QString::number(WeatherUtils::Miles));
    }
    return visibilityInfo;
}

QMap<QString, QString> NOAAIon::temperature(const QString& source)
{
    QMap<QString, QString> temperatureInfo;
    temperatureInfo.insert("temperature", d->m_weatherData[source].temperature_F);
    temperatureInfo.insert("temperatureUnit", QString::number(WeatherUtils::Fahrenheit));
    temperatureInfo.insert("comfortTemperature", "N/A");

    if (d->m_weatherData[source].heatindex_F != "NA" && d->m_weatherData[source].windchill_F == "NA") {
        temperatureInfo.insert("comfortTemperature", d->m_weatherData[source].heatindex_F);
    }

    if (d->m_weatherData[source].windchill_F != "NA" && d->m_weatherData[source].heatindex_F == "NA") {
        temperatureInfo.insert("comfortTemperature", d->m_weatherData[source].windchill_F);
    }

    return temperatureInfo;
}

QMap<QString, QString> NOAAIon::pressure(const QString& source)
{
    QMap<QString, QString> pressureInfo;
    if (d->m_weatherData[source].pressure.isEmpty()) {
        pressureInfo.insert("pressure", "N/A");
        return pressureInfo;
    }

    if (d->m_weatherData[source].pressure == "NA") {
        pressureInfo.insert("pressure", "N/A");
        pressureInfo.insert("visibilityUnit", QString::number(WeatherUtils::NoUnit));
    } else {
        pressureInfo.insert("pressure", d->m_weatherData[source].pressure);
        pressureInfo.insert("pressureUnit", QString::number(WeatherUtils::InchesHG));
    }
    return pressureInfo;
}

QMap<QString, QString> NOAAIon::wind(const QString& source)
{
    QMap<QString, QString> windInfo;

    // May not have any winds
    if (d->m_weatherData[source].windSpeed == "NA") {
        windInfo.insert("windSpeed", i18nc("wind speed", "Calm"));
        windInfo.insert("windUnit", QString::number(WeatherUtils::NoUnit));
    } else {
        windInfo.insert("windSpeed", QString::number(d->m_weatherData[source].windSpeed.toFloat(), 'f', 1));
        windInfo.insert("windUnit", QString::number(WeatherUtils::MilesPerHour));
    }

    // May not always have gusty winds
    if (d->m_weatherData[source].windGust == "NA" || d->m_weatherData[source].windGust == "N/A") {
        windInfo.insert("windGust", "N/A");
        windInfo.insert("windGustUnit", QString::number(WeatherUtils::NoUnit));
    } else {
        windInfo.insert("windGust", QString::number(d->m_weatherData[source].windGust.toFloat(), 'f', 1));
        windInfo.insert("windGustUnit", QString::number(WeatherUtils::MilesPerHour));
    }

    if (d->m_weatherData[source].windDirection.isEmpty()) {
        windInfo.insert("windDirection", "N/A");
    } else {
        windInfo.insert("windDirection", i18nc("wind direction", d->m_weatherData[source].windDirection.toUtf8()));
    }
    return windInfo;
}

#include "ion_noaa.moc"
