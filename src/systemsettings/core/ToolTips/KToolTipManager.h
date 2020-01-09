/***************************************************************************
 *   Copyright (C) 2008 by Fredrik Höglund <fredrik@kde.org>               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#ifndef KTOOLTIPMANAGER_H
#define KTOOLTIPMANAGER_H

#include <QSharedData>

#include <KSharedPtr>

#include "KTipLabel.h"
#include "KStyleOptionToolTip.h"
#include "KToolTipDelegate.h"

class KToolTipManager : public QSharedData
{
public:
    ~KToolTipManager();

    static KSharedPtr<KToolTipManager> instance() {
        if (!s_instance)
            s_instance = new KToolTipManager();

        return KSharedPtr<KToolTipManager>(s_instance);
    }

    void showTip(const QPoint &pos, KToolTipItem *item);
    void hideTip();

    void initStyleOption(KStyleOptionToolTip *option) const;

    void setDelegate(KToolTipDelegate *delegate);
    KToolTipDelegate *delegate() const;
    
    void update();

private:
    KToolTipManager();

    KTipLabel *label;
    KToolTipItem *currentItem;
    KToolTipDelegate *m_delegate;

    QPoint m_tooltipPos;

    static KToolTipManager *s_instance;
};

#endif
