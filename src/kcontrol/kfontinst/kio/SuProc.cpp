/*
 * KFontInst - KDE Font Installer
 *
 * Copyright 2003-2007 Craig Drummond <craig@kde.org>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "SuProc.h"
#include "KfiConstants.h"
#include <QtCore/QFile>
#include <KDE/KShell>
#include <KDE/KStandardDirs>
#include <KDE/SuProcess>
#include <KDE/KDebug>
#include <unistd.h>
#include <config-workspace.h>

namespace KFI
{

CSuProc::CSuProc(QByteArray &sock, QString &passwd)
       : itsPasswd(passwd)
{
    QString exe(KStandardDirs::findExe(QLatin1String("kio_fonts_helper"), KStandardDirs::installPath("libexec")));

    if(exe.isEmpty())
        kError(7000) << "Could not locate kio_fonts_helper";
    else
    {
        itsCmd=QByteArray(QFile::encodeName(KShell::quoteArg(exe)));

        itsCmd+=' ';
        itsCmd+=QFile::encodeName(KShell::quoteArg(sock));
        itsCmd+=' ';
        itsCmd+=QString().setNum(getuid()).toLatin1();
    }
}

void CSuProc::run()
{
    if(!itsCmd.isEmpty())
    {
        KDESu::SuProcess proc(KFI_SYS_USER);
        proc.setCommand(itsCmd);
        proc.exec(itsPasswd.toLocal8Bit());
    }
}

}
