/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2018 UniPro <ugene@unipro.ru>
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

#include "StringTieTask.h"
#include "StringTieSupport.h"

namespace U2 {

StringTieTask::StringTieTask(const StringTieTaskSettings& settings)
    : ExternalToolSupportTask(tr("Assemble Transcripts with StringTie task"), TaskFlags_NR_FOSE_COSC),
      settings(settings){

}

void StringTieTask::prepare() {
    stringTieTask = new ExternalToolRunTask(ET_STRINGTIE, getArguments(), new ExternalToolLogParser());
    setListenerForTask(stringTieTask);
    addSubTask(stringTieTask);
}

const StringTieTaskSettings& StringTieTask::getSettings() const {
    return settings;
}

QStringList StringTieTask::getArguments() const {
    QStringList arguments;
    arguments << settings.inputBam;
    if (!settings.readOrientation.isEmpty()) {
        arguments << settings.readOrientation;
    }
    arguments << "-l" << settings.label;
    arguments << "-f" << QString::number(settings.minIsoformFraction);
    arguments << "-m" << QString::number(settings.minTransciptLen);
    arguments << "-a" << QString::number(settings.minAnchorLen);
    arguments << "-j" << QString::number(settings.minJunctionCoverage);
    if (settings.trimTranscript) {
        arguments << "-t";
    }
    arguments << "-c" << QString::number(settings.minCoverage);
    arguments << "-g" << QString::number(settings.minLocusSeparation);
    arguments << "-M" << QString::number(settings.multiHitFraction);
    if (!settings.skipSequences.isEmpty()) {
        // check format
        arguments << "-x" << settings.skipSequences;
    }
    if (settings.refOnlyAbudance) {
        arguments << "-e";
    }
    if (settings.multiMappingCorrection) {
        arguments << "-u";
    }
    if (settings.verboseLog) {
        arguments << "-v";
    }
    arguments << "-p" << QString::number(settings.threadNum);
    arguments << "-o" << settings.primaryOutputFile;
    if (settings.geneAbundanceOutput) {
        arguments << "-A" << settings.geneAbundanceOutputFile;
    }
    if (settings.coveredRefOutput) {
        arguments << "-C" << settings.coveredRefOutputFile;
    }
    if (settings.ballgownOutput) {
        arguments << "-b" << settings.ballgowmOutputFolder;
    }

    return arguments;
}

} // namespace
