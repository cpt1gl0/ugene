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

#include "FindPatternMsaTask.h"
#include "../../ov_sequence/find_pattern/FindPatternTask.h"

#include <U2Core/MultipleSequenceAlignmentObject.h>
#include <U2Core/MultiTask.h>

namespace U2 {

FindPatternMsaTask::FindPatternMsaTask() : Task(tr("Searching a pattern in multiple alignment task"), TaskFlags_NR_FOSE_COSC),
    searchMultitask(nullptr) {

}

FindPatternMsaTask::FindPatternMsaTask(const FindPatternMsaSettings& _settings)
    : Task(tr("Searching a pattern in multiple alignment task"), TaskFlags_NR_FOSE_COSC),
    settings(_settings) {
}

void FindPatternMsaTask::prepare() {
    QList<Task*> tasks;
    FindAlgorithmTaskSettings algoSettings;
    algoSettings.searchIsCircular = false;
    algoSettings.strand = FindAlgorithmStrand_Direct;
    algoSettings.maxResult2Find = settings.findSettings.maxResult2Find;
    algoSettings.useAmbiguousBases = false;
    algoSettings.patternSettings = settings.findSettings.patternSettings;
    algoSettings.sequenceAlphabet = settings.msaObj->getAlphabet();
    for (int i = 0; i < settings.msaObj->getNumRows(); i++) {
        QByteArray seq = settings.msaObj->getRow(i)->getUngeppedDnaSequence().constSequence();
        FindAlgorithmTaskSettings currentSettings = algoSettings;
        currentSettings.sequence = seq;
        currentSettings.searchRegion = U2Region(0, seq.length());
        tasks.append(new FindPatternListTask(currentSettings, settings.patterns, settings.removeOverlaps , settings.matchValue));
    }
    searchMultitask = new MultiTask(tr("Search in Multiple Sequence Alignment multitask"), tasks, false, TaskFlags_NR_FOSE_COSC);
    addSubTask(searchMultitask);
}

QList<Task*> FindPatternMsaTask::onSubTaskFinished(Task* subTask) {
    QList<Task*> result;
    if (subTask->hasError() && subTask == searchMultitask) {
        stateInfo.setError(subTask->getError());
        return result;
    }

    if (subTask == searchMultitask) {
        int index = 0;
        foreach(Task * searchTask, searchMultitask->getTasks()) {
            FindPatternListTask* task = qobject_cast<FindPatternListTask*>(searchTask);
            SAFE_POINT(task, "Failed to cast FindPatternListTask!", QList<Task*>());
            if (!task->getResults().isEmpty()) {
                QList<U2Region> resultRegions;
                foreach(const SharedAnnotationData & data, task->getResults()) {
                    QList<U2Region> gappedRegionList;
                    foreach(const U2Region &region, data->getRegions().toList()) {
                        resultRegions.append(settings.msaObj->getMultipleAlignment()->getRow(index).data()->getGapped(region));
                    }
                    resultRegions.append(gappedRegionList);
                }
                if (settings.findSettings.patternSettings == FindAlgorithmPatternSettings_RegExp) { //Other algos always return sorted results
                    qSort(resultRegions.begin(), resultRegions.end());
                }
                resultsBySeqIndex.insert(index, resultRegions);
            }
            index++;
        }
    }
    return result;
}

const QMap<int, QList<U2::U2Region> >& FindPatternMsaTask::getResults() const {
    return resultsBySeqIndex;
}

}