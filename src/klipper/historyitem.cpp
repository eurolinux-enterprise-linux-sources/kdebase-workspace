// -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*-
/* This file is part of the KDE project
   Copyright (C) 2004  Esben Mose Hansen <kde@mosehansen.dk>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include <QMap>

#include <QPixmap>

#include <kdebug.h>

#include "historyitem.h"
#include "historystringitem.h"
#include "historyimageitem.h"
#include "historyurlitem.h"

HistoryItem::HistoryItem() {

}

HistoryItem::~HistoryItem() {

}

HistoryItem* HistoryItem::create( const QMimeData* data )
{
#if 0
    int i=0;
    while ( const char* f = aSource.format( i++ ) ) {
        kDebug() << "format(" << i <<"): " << f;
    }
#endif
    if (KUrl::List::canDecode(data))
    {
        KUrl::MetaDataMap metaData;
        KUrl::List urls = KUrl::List::fromMimeData(data, &metaData);
        QByteArray a = data->data("application/x-kde-cutselection");
        bool cut = !a.isEmpty() && (a.at(0) == '1'); // true if 1
        return new HistoryURLItem(urls, metaData, cut);
    }
    if (data->hasText())
    {
        return new HistoryStringItem(data->text());
    }
    if (data->hasImage())
    {
        QImage image = qvariant_cast<QImage>(data->imageData());
        return new HistoryImageItem(QPixmap::fromImage(image));
    }

    return 0; // Failed.
}

HistoryItem* HistoryItem::create( QDataStream& aSource ) {
    if ( aSource.atEnd() ) {
        return 0;
    }
    QString type;
    aSource >> type;
    if ( type == "url" ) {
        KUrl::List urls;
        QMap< QString, QString > metaData;
        int cut;
        aSource >> urls;
        aSource >> metaData;
        aSource >> cut;
        return new HistoryURLItem( urls, metaData, cut );
    }
    if ( type == "string" ) {
        QString text;
        aSource >> text;
        return new HistoryStringItem( text );
    }
    if ( type == "image" ) {
        QPixmap image;
        aSource >> image;
        return new HistoryImageItem( image );
    }
    kWarning() << "Failed to restore history item: Unknown type \"" << type << "\"" ;
    return 0;
}

