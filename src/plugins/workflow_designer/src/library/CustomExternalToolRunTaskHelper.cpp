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

#include "CustomExternalToolRunTaskHelper.h"

#include <U2Core/U2SafePoints.h>

namespace U2 {    

CustomExternalToolRunTaskHelper::CustomExternalToolRunTaskHelper(QProcess* process, ExternalToolLogParser* logParser, U2OpStatus& os) : ExternalToolRunTaskHelper(process, logParser, os) {}

void CustomExternalToolRunTaskHelper::sl_onReadyToReadErrLog() {
    QMutexLocker locker(&logMutex);

    CHECK(NULL != process, );
    if (process->readChannel() == QProcess::StandardOutput) {
        process->setReadChannel(QProcess::StandardError);
    }
    int numberReadChars = process->read(logData.data(), logData.size());
    while (numberReadChars > 0) {
        //call log parser
        QString line = QString::fromLocal8Bit(logData.constData(), numberReadChars);
        logParser->parseErrOutput(line);
        if (NULL != listener) {
            listener->addNewLogMessage(line, ExternalToolListener::ERROR_LOG);
        }
        numberReadChars = process->read(logData.data(), logData.size());
    }
    os.setProgress(logParser->getProgress());
}

}
