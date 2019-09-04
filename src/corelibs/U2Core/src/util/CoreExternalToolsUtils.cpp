/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2019 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "CoreExternalToolsUtils.h"

#include <QFileInfo>

namespace U2 {

const QMap<QString, QString> CoreExternalToolsUtils::extToToolMap = { {"py", ET_PYTHON_ID}, {"pl", ET_PERL_ID} };
const QString CoreExternalToolsUtils::ET_PERL_ID = "UGENE_PERL";
const QString CoreExternalToolsUtils::ET_PYTHON_ID = "UGENE_PYTHON2";

const QString& CoreExternalToolsUtils::detectLauncherByExtension(const QString& toolPath) {
    QFileInfo path(toolPath);
    return extToToolMap[path.suffix()];
}

}