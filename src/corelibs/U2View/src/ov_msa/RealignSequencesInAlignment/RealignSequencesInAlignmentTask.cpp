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
#include <QDirIterator>
#include <QUuid>

#include <U2Algorithm/CreateSubalignmentTask.h>

#include <U2Core/AppContext.h>
#include <U2Core/AppSettings.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/GUrlUtils.h>
#include <U2Core/StateLockableDataModel.h>
#include <U2Core/U2Dbi.h>
#include <U2Core/U2ObjectDbi.h>
#include <U2Core/U2Mod.h>
#include <U2Core/UserApplicationsSettings.h>

#include "RealignSequencesInAlignmentTask.h"
#include "../AlignSequencesToAlignment/AlignSequencesToAlignmentTask.h"
#include "../ExportSequencesTask.h"

namespace U2 {

RealignSequencesInAlignmentTask::RealignSequencesInAlignmentTask(MultipleSequenceAlignmentObject* msaObjectToClone, const QSet<qint64>& _rowsToAlignIds)
    : Task(tr("Realign sequences in this alignment"), TaskFlags_NR_FOSE_COSC),
    originalMsaObject(msaObjectToClone),
    msaObject(nullptr),
    rowsToAlignIds(_rowsToAlignIds)
{
    locker = new StateLocker(originalMsaObject);
    msaObject = msaObjectToClone->clone(msaObjectToClone->getEntityRef().dbiRef, stateInfo);
    CHECK_OP(stateInfo, );

    foreach(const MultipleAlignmentRow& row, msaObject->getRows()) {
        originalRowOrder.append(row->getName());
    }

    CreateSubalignmentSettings settings;
    settings.window = U2Region(0, msaObject->getLength());
    
    QList<qint64> sequencesToKeepIds = msaObject->getMultipleAlignment()->getRowsIds();
    foreach(const qint64 idToRemove, rowsToAlignIds) {
        sequencesToKeepIds.removeAll(idToRemove);
    }
    settings.rowIds = sequencesToKeepIds;

    QString url;
    QString path = AppContext::getAppSettings()->getUserAppsSettings()->getCurrentProcessTemporaryDirPath();
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(path);
    }

    extractedSequencesDirUrl = path + "/tmp" + GUrlUtils::fixFileName(QUuid::createUuid().toString());
    dir = QDir(extractedSequencesDirUrl);    
    dir.mkpath(extractedSequencesDirUrl);

    extractSequences = new ExportSequencesTask(msaObject->getMsa(), rowsToAlignIds, false, false, extractedSequencesDirUrl, BaseDocumentFormats::FASTA, "fa");
    addSubTask(extractSequences);
}

RealignSequencesInAlignmentTask::~RealignSequencesInAlignmentTask() {
    delete msaObject;
}

U2::Task::ReportResult RealignSequencesInAlignmentTask::report() {
    msaObject->sortRowsByList(originalRowOrder);
    delete locker;
    locker = nullptr;
    U2UseCommonUserModStep modStep(originalMsaObject->getEntityRef(), stateInfo);
    CHECK_OP(stateInfo, Task::ReportResult_Finished);
    originalMsaObject->updateGapModel(msaObject->getMsa()->getMsaRows());
    QDir tmpDir(extractedSequencesDirUrl);
    foreach(const QString & file, tmpDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        tmpDir.remove(file);
    }
    tmpDir.rmdir(tmpDir.absolutePath());
    
    DbiConnection con(msaObject->getEntityRef().dbiRef, stateInfo);
    CHECK_OP(stateInfo, Task::ReportResult_Finished);
    CHECK(con.dbi->getFeatures().contains(U2DbiFeature_RemoveObjects), Task::ReportResult_Finished);
    con.dbi->getObjectDbi()->removeObject(msaObject->getEntityRef().entityId, true, stateInfo);   

    return Task::ReportResult_Finished;
}

QList<Task*> RealignSequencesInAlignmentTask::onSubTaskFinished(Task* subTask) {
    QList<Task*> res;
    CHECK_OP(stateInfo, res);
    
    if (subTask == extractSequences) {
        foreach(qint64 idToRemove, rowsToAlignIds) {
            msaObject->removeRow(msaObject->getRowPosById(idToRemove));
        }
        QStringList sequenceFilesToAlign;
        QDirIterator it(extractedSequencesDirUrl, QStringList() << "*.fa", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            sequenceFilesToAlign.append(it.next());
        }

        LoadSequencesAndAlignToAlignmentTask* task = new LoadSequencesAndAlignToAlignmentTask(msaObject, sequenceFilesToAlign);
        res.append(task);
    }

    return res;
}

}