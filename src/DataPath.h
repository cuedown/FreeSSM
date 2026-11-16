/*
 * DataPath.h - location of FreeSSM's data files (definitions, translations, help, fonts)
 *
 * Windows/Linux: next to the executable (unchanged behaviour).
 * macOS: inside the app bundle's Resources folder, because code signing
 *        rejects non-code files in Contents/MacOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DATAPATH_H
#define DATAPATH_H

#include <QCoreApplication>
#include <QString>

inline QString fssmDataPath()
{
#ifdef __APPLE__
	return QCoreApplication::applicationDirPath() + "/../Resources";
#else
	return QCoreApplication::applicationDirPath();
#endif
}

#endif
