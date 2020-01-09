#include <kdesktopfile.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include "saverconfig.h"

SaverConfig::SaverConfig()
{
}

bool SaverConfig::read(const QString &file)
{
    KDesktopFile config(file);
    const KConfigGroup group = config.desktopGroup();
#if 0
    if( !config.tryExec())
      return false;
#endif
    mExec = group.readPathEntry("Exec", QString());
    mName = group.readEntry("Name");
    QString categoryName = group.readEntry("X-KDE-Category");
    if(categoryName.isEmpty())
	mCategory = categoryName;
    else
        mCategory = i18nc("Screen saver category", // Must be same in CMakeFiles.txt
                     categoryName.toUtf8());

    if (config.hasActionGroup("Setup"))
    {
      mSetup = config.actionGroup("Setup").readPathEntry("Exec", QString());
    }

    if (config.hasActionGroup("InWindow"))
    {
      mSaver = config.actionGroup("InWindow").readPathEntry("Exec", QString());
    }

    int indx = file.lastIndexOf('/');
    if (indx >= 0) {
        mFile = file.mid(indx+1);
    }

    return !mSaver.isEmpty();
}
