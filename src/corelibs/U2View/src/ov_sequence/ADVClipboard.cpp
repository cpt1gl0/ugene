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

#include "ADVClipboard.h"

#include "AnnotatedDNAView.h"
#include "ADVSequenceObjectContext.h"
#include "ADVConstants.h"

#include <U2Core/DNATranslation.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/DNASequenceSelection.h>
#include <U2Core/AnnotationSelection.h>
#include <U2Core/L10n.h>
#include <U2Core/SelectionUtils.h>
#include <U2Core/SequenceUtils.h>
#include <U2Core/U2SequenceUtils.h>
#include <U2Core/TextUtils.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Gui/GUIUtils.h>

#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace U2 {

const QString ADVClipboard::COPY_FAILED_MESSAGE = QApplication::translate("ADVClipboard", "Cannot put sequence data into the clipboard buffer.\n"
                                                                        "Probably, the data are too big.");
const qint64 ADVClipboard::MAX_COPY_SIZE_FOR_X86 = 100 * 1024 * 1024;

ADVClipboard::ADVClipboard(AnnotatedDNAView* c) : QObject(c) {
    ctx = c;
    //TODO: listen seqadded/seqremoved!!

    connect(ctx, SIGNAL(si_focusChanged(ADVSequenceWidget*, ADVSequenceWidget*)),
                 SLOT(sl_onFocusedSequenceWidgetChanged(ADVSequenceWidget*, ADVSequenceWidget*)));

    foreach(ADVSequenceObjectContext* sCtx, ctx->getSequenceContexts()) {
        connectSequence(sCtx);
    }

    copySequenceAction = new QAction(QIcon(":/core/images/copy_sequence.png"), tr("Copy sequence"), this);
    copySequenceAction->setObjectName("Copy sequence");
    copySequenceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));

    copyTranslationAction = new QAction(QIcon(":/core/images/copy_translation.png"), tr("Copy translation"), this);
    copyTranslationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    copyTranslationAction->setObjectName(ADV_COPY_TRANSLATION_ACTION);

    copyComplementSequenceAction = new QAction(QIcon(":/core/images/copy_complement_sequence.png"), tr("Copy reverse-complement sequence"), this);
    copyComplementSequenceAction->setObjectName("Copy reverse complement sequence");
    copyComplementSequenceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));

    copyComplementTranslationAction = new QAction(QIcon(":/core/images/copy_complement_translation.png"), tr("Copy reverse-complement translation"), this);
    copyComplementTranslationAction->setObjectName("Copy reverse complement translation");
    copyComplementTranslationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));

    copyAnnotationSequenceAction = new QAction(QIcon(":/core/images/copy_annotation_sequence.png"), tr("Copy annotation sequence"), this);
    copyAnnotationSequenceAction->setObjectName("action_copy_annotation_sequence");
    copyAnnotationSequenceTranslationAction = new QAction(QIcon(":/core/images/copy_annotation_translation.png"), tr("Copy annotation sequence translation"), this);
    copyAnnotationSequenceTranslationAction->setObjectName("Copy annotation sequence translation");

    pasteSequenceAction = createPasteSequenceAction(this);

    connect(copySequenceAction, SIGNAL(triggered()), SLOT(sl_copySequence()));
    connect(copyTranslationAction, SIGNAL(triggered()), SLOT(sl_copyTranslation()));
    connect(copyComplementSequenceAction, SIGNAL(triggered()), SLOT(sl_copyComplementSequence()));
    connect(copyComplementTranslationAction, SIGNAL(triggered()), SLOT(sl_copyComplementTranslation()));
    connect(copyAnnotationSequenceAction, SIGNAL(triggered()), SLOT(sl_copyAnnotationSequence()));
    connect(copyAnnotationSequenceTranslationAction, SIGNAL(triggered()), SLOT(sl_copyAnnotationSequenceTranslation()));

    updateActions();
}

void ADVClipboard::connectSequence(ADVSequenceObjectContext *sCtx) {
    connect(sCtx->getSequenceSelection(),
      SIGNAL(si_selectionChanged(LRegionsSelection *, const QVector<U2Region> &, const QVector<U2Region> &)),
      SLOT(sl_onDNASelectionChanged(LRegionsSelection *, const QVector<U2Region> &, const QVector<U2Region> &)));

    connect(sCtx->getAnnotatedDNAView()->getAnnotationsSelection(),
      SIGNAL(si_selectionChanged(AnnotationSelection *, const QList<Annotation *> &, const QList<Annotation *> &)),
      SLOT(sl_onAnnotationSelectionChanged(AnnotationSelection *, const QList<Annotation *> &, const QList<Annotation *> &)));
}

void ADVClipboard::sl_onDNASelectionChanged(LRegionsSelection *, const QVector<U2Region> &, const QVector<U2Region> &) {
    updateActions();
}

void ADVClipboard::sl_onAnnotationSelectionChanged(AnnotationSelection *, const QList<Annotation *> &, const QList<Annotation *> &) {
    updateActions();
}

void ADVClipboard::copySequenceSelection(bool complement, bool amino) {
    ADVSequenceObjectContext* seqCtx = getSequenceContext();
    if (seqCtx == NULL) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), "No sequence selected!");
        return;
    }

    QString res;
    QVector<U2Region> regions = seqCtx->getSequenceSelection()->getSelectedRegions();
#ifdef UGENE_X86
    int totalLen = 0;
    foreach (const U2Region& r, regions) {
        totalLen += r.length;
    }
    if (totalLen > MAX_COPY_SIZE_FOR_X86) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), COPY_FAILED_MESSAGE);
        return;
    }
 #endif

    if (!regions.isEmpty()) {
        U2SequenceObject* seqObj = seqCtx->getSequenceObject();
        DNATranslation* complTT = complement ? seqCtx->getComplementTT() : NULL;
        DNATranslation* aminoTT = amino ? seqCtx->getAminoTT() : NULL;
        U2OpStatus2Log os;
        QList<QByteArray> seqParts = U2SequenceUtils::extractRegions(seqObj->getSequenceRef(), regions, complTT, aminoTT, false, os);
        if (os.hasError()) {
            QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), tr("An error occurred during getting sequence data: %1").arg(os.getError()));
            return;
        }
        if (seqParts.size() == 1) {
            putIntoClipboard(seqParts.first());
            return;
        }
        res = U1SequenceUtils::joinRegions(seqParts);
    }
    putIntoClipboard(res);
}

void ADVClipboard::putIntoClipboard(const QString& data) {
    CHECK(!data.isEmpty(), );
#ifdef UGENE_X86
    if (data.size() > MAX_COPY_SIZE_FOR_X86) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), COPY_FAILED_MESSAGE);
        return;
    }
#endif
    try {
        QApplication::clipboard()->setText(data);
    } catch (...) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), COPY_FAILED_MESSAGE);
    }
}

void ADVClipboard::sl_copySequence() {
    copySequenceSelection(false, false);
}

void ADVClipboard::sl_copyTranslation() {
    copySequenceSelection(false, true);
}

void ADVClipboard::sl_copyComplementSequence() {
    copySequenceSelection(true, false);
}

void ADVClipboard::sl_copyComplementTranslation() {
    copySequenceSelection(true, true);
}


void ADVClipboard::sl_copyAnnotationSequence() {
    QByteArray res;
    const QList<Annotation*>& as = ctx->getAnnotationsSelection()->getAnnotations();
#ifdef UGENE_X86
    qint64 totalLen = 0;
    foreach (const Annotation* annotation, as) {
        totalLen += annotation->getRegionsLen();
    }
    if (totalLen > MAX_COPY_SIZE_FOR_X86) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), COPY_FAILED_MESSAGE);
        return;
    }
#endif

    //BUG528: add alphabet symbol role: insertion mark
    const char gapSym = '-';
    const int size = as.size();
    for (int i = 0; i < size; i++) {
        const Annotation* annotation = as.at(i);
        if (i!=0) {
            res.append('\n');
        }
        ADVSequenceObjectContext* seqCtx = ctx->getSequenceContext(annotation->getGObject());
        if (seqCtx == NULL) {
            res.append(gapSym);//?? generate sequence with len == region-len using default sym?
            continue;
        }
        DNATranslation* complTT = annotation->getStrand().isCompementary() ? seqCtx->getComplementTT() : NULL;
        U2OpStatus2Log os;
        AnnotationSelection::getAnnotationSequence(res, annotation, gapSym, seqCtx->getSequenceRef(), complTT, NULL, os);
        CHECK_OP(os,);
    }
    putIntoClipboard(res);
}


void ADVClipboard::sl_copyAnnotationSequenceTranslation() {
    QByteArray res;
    const QList<Annotation*>& as = ctx->getAnnotationsSelection()->getAnnotations();

#ifdef UGENE_X86
    qint64 totalLen = 0;
    foreach (const Annotation* a, as) {
        totalLen += a->getRegionsLen();
    }
    if (totalLen > MAX_COPY_SIZE_FOR_X86) {
        QMessageBox::critical(QApplication::activeWindow(), L10N::errorTitle(), COPY_FAILED_MESSAGE);
        return;
    }
#endif

    //BUG528: add alphabet symbol role: insertion mark
    //TODO: reuse AnnotationSelection utils
    const char gapSym = '-';
    const int size = as.size();
    for (int i = 0; i < size; i++) {
        const Annotation* annotation = as.at(i);
        if (i != 0) {
            res.append('\n');
        }
        ADVSequenceObjectContext* seqCtx = ctx->getSequenceContext(annotation->getGObject());
        if (seqCtx == NULL) {
            res.append(gapSym);//?? generate sequence with len == region-len using default sym?
            continue;
        }
        DNATranslation *complTT = annotation->getStrand().isCompementary() ? seqCtx->getComplementTT() : NULL;
        DNATranslation *aminoTT = seqCtx->getAminoTT();
        if (aminoTT == NULL) {
            continue;
        }
        U2OpStatus2Log os;
        QList<QByteArray> parts = U2SequenceUtils::extractRegions(seqCtx->getSequenceRef(), annotation->getRegions(), complTT,
            aminoTT, annotation->isJoin(), os);
        CHECK_OP(os,);
        res += U1SequenceUtils::joinRegions(parts);
    }
    putIntoClipboard(res);
}

void ADVClipboard::updateActions() {
    ADVSequenceObjectContext* seqCtx = getSequenceContext();
    DNASequenceSelection* sel = seqCtx == NULL ? NULL : seqCtx->getSequenceSelection();
    bool hasSequence = sel != NULL;
    bool hasComplement = hasSequence && seqCtx->getComplementTT()!=NULL;
    bool hasTranslation = hasSequence && seqCtx->getAminoTT()!=NULL;
    bool selectionIsNotEmpty = hasSequence && !sel->getSelectedRegions().isEmpty();

    copySequenceAction->setEnabled(selectionIsNotEmpty);
    copySequenceAction->setShortcut(selectionIsNotEmpty ? QKeySequence::Copy : QKeySequence());
    copyTranslationAction->setEnabled(selectionIsNotEmpty && hasTranslation);
    copyComplementSequenceAction->setEnabled(selectionIsNotEmpty && hasComplement);
    copyComplementTranslationAction->setEnabled(selectionIsNotEmpty && hasComplement && hasTranslation);

    bool hasAnnotationSelection = !ctx->getAnnotationsSelection()->isEmpty();
    bool hasSequenceForAnnotations = false;
    bool hasTranslationForAnnotations = false;
    if (hasAnnotationSelection) {
        const QList<Annotation*>& as = ctx->getAnnotationsSelection()->getAnnotations();
        foreach(const Annotation* annotation, as) {
            ADVSequenceObjectContext* asCtx = ctx->getSequenceContext(annotation->getGObject());
            if (asCtx == NULL) {
                continue;
            }
            hasSequenceForAnnotations = true;
            hasTranslationForAnnotations = hasTranslationForAnnotations || asCtx->getAminoTT()!=NULL;
            if (hasTranslationForAnnotations) {
                break;
            }
        }
    }
    copyAnnotationSequenceAction->setEnabled(hasAnnotationSelection && hasSequenceForAnnotations);
    copyAnnotationSequenceAction->setShortcut(selectionIsNotEmpty ? QKeySequence() : QKeySequence::Copy);
    copyAnnotationSequenceTranslationAction->setEnabled(hasAnnotationSelection && hasTranslationForAnnotations);
}


void ADVClipboard::addCopyMenu(QMenu* m) {
    QMenu* copyMenu = new QMenu(tr("Copy/Paste"), m);
    copyMenu->menuAction()->setObjectName(ADV_MENU_COPY);

    copyMenu->addAction(copySequenceAction);
    copyMenu->addAction(copyComplementSequenceAction);
    copyMenu->addAction(copyTranslationAction);
    copyMenu->addAction(copyComplementTranslationAction);
    copyMenu->addAction(copyAnnotationSequenceAction);
    copyMenu->addAction(copyAnnotationSequenceTranslationAction);
    copyMenu->addAction(pasteSequenceAction);

    m->addMenu(copyMenu);
}

QAction * ADVClipboard::createPasteSequenceAction(QObject *parent) {
    QAction *action = new QAction(QIcon(":/core/images/paste.png"), tr("Paste sequence"), parent);
    action->setObjectName("Paste sequence");
    action->setShortcuts(QKeySequence::Paste);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    return action;
}

ADVSequenceObjectContext* ADVClipboard::getSequenceContext() const {
    return ctx->getSequenceInFocus();
}

void ADVClipboard::sl_onFocusedSequenceWidgetChanged(ADVSequenceWidget* oldW, ADVSequenceWidget* newW) {
    Q_UNUSED(oldW);
    Q_UNUSED(newW);
    updateActions();
}
}//namespace
