/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2011 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
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

#include "AssemblyReadsArea.h"

#include <assert.h>
#include <math.h>

#include <QtGui/QVBoxLayout>
#include <QtGui/QPainter>
#include <QtGui/QCursor>
#include <QtGui/QResizeEvent>
#include <QtGui/QWheelEvent>

#include <U2Core/U2AssemblyUtils.h>
#include <U2Core/Counter.h>
#include <U2Core/Timer.h>
#include <U2Core/Log.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/FormatUtils.h>

#include "AssemblyBrowser.h"
#include "ShortReadIterator.h"
#include "ZoomableAssemblyOverview.h"

namespace U2 {

AssemblyReadsArea::AssemblyReadsArea(AssemblyBrowserUi * ui_, QScrollBar * hBar_, QScrollBar * vBar_) : 
QWidget(ui_), ui(ui_), browser(ui_->getWindow()), model(ui_->getModel()), scribbling(false), redraw(true),
coveredRegionsLabel(this), hBar(hBar_), vBar(vBar_), hintData(this) {
    QVBoxLayout * coveredRegionsLayout = new QVBoxLayout();
    coveredRegionsLayout->addWidget(&coveredRegionsLabel);
    setLayout(coveredRegionsLayout);
    initRedraw();
    connectSlots();
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void AssemblyReadsArea::initRedraw() {
    redraw = true;
    cachedView = QPixmap(size());
}

void AssemblyReadsArea::connectSlots() {
    connect(browser, SIGNAL(si_zoomOperationPerformed()), SLOT(sl_zoomOperationPerformed()));
    connect(browser, SIGNAL(si_offsetsChanged()), SLOT(sl_redraw()));
    connect(&coveredRegionsLabel, SIGNAL(linkActivated(const QString&)), SLOT(sl_coveredRegionClicked(const QString&)));
}   

void AssemblyReadsArea::setupHScrollBar() {
    U2OpStatusImpl status;
    hBar->disconnect(this);

    qint64 assemblyLen = model->getModelLength(status);
    qint64 numVisibleBases = browser->basesVisible();

    hBar->setMinimum(0);
    hBar->setMaximum(assemblyLen - numVisibleBases + 1); //TODO: remove +1
    hBar->setSliderPosition(browser->getXOffsetInAssembly());

    hBar->setSingleStep(1);
    hBar->setPageStep(numVisibleBases);

    hBar->setDisabled(numVisibleBases == assemblyLen);

    connect(hBar, SIGNAL(valueChanged(int)), SLOT(sl_onHScrollMoved(int)));
}

void AssemblyReadsArea::setupVScrollBar() {
    U2OpStatusImpl status;
    vBar->disconnect(this);

    qint64 assemblyHeight = model->getModelHeight(status);
    qint64 numVisibleRows = browser->rowsVisible();

    vBar->setMinimum(0);
    vBar->setMaximum(assemblyHeight - numVisibleRows + 2); //TODO: remove +2
    vBar->setSliderPosition(browser->getYOffsetInAssembly());

    vBar->setSingleStep(1);
    vBar->setPageStep(numVisibleRows);

    if(numVisibleRows == assemblyHeight) {
        vBar->setDisabled(true);
        //vBar->hide(); TODO: do hide(), but prevent infinite resizing (hide->show->hide->show) caused by width change
    } else {
        vBar->setDisabled(false);
        //vBar->show();
    }

    connect(vBar, SIGNAL(valueChanged(int)), SLOT(sl_onVScrollMoved(int)));
}

void AssemblyReadsArea::drawAll() {
    GTIMER(c1, t1, "AssemblyReadsArea::drawAll");
    if(!model->isEmpty()) {
        if (redraw) {
            cachedView.fill(Qt::transparent);
            QPainter p(&cachedView);
            redraw = false;

            if(!browser->areReadsVisible()) {
                drawWelcomeScreen(p);
            } else {
                drawReads(p);
            }
            setupHScrollBar(); 
            setupVScrollBar();
        }
        QPixmap cachedViewCopy(cachedView);
        if(hintData.redrawHint) {
            QPainter p(&cachedViewCopy);
            hintData.redrawHint = false;
            drawHint(p);
        }
        QPainter p(this);
        p.drawPixmap(0, 0, cachedViewCopy);
    }
}

void AssemblyReadsArea::drawWelcomeScreen(QPainter & p) {
    GTIMER(c1, t1, "AssemblyReadsArea::drawDensityGraph");

    cachedReads.clear();
    QString text = tr("Zoom in to see the reads");

    QList<CoveredRegion> coveredRegions = browser->getCoveredRegions();
    if(!coveredRegions.empty()) {
        text += tr(" or choose one of the well-covered regions:<br><br>");
        QString coveredRegionsText = "<table align=\"center\" cellspacing=\"2\">";
        /*
        * |   | Region | Coverage |
        * | 1 | [x,y]  | z        |
        */
        coveredRegionsText += tr("<tr><td></td><td>Region</td><td>Coverage</td></tr>");
        for(int i = 0; i < coveredRegions.size(); ++i) {
            const CoveredRegion & cr = coveredRegions.at(i);
            QString crStart = FormatUtils::splitThousands(cr.region.startPos);
            QString crEnd = FormatUtils::splitThousands(cr.region.endPos());
            QString crCoverage = FormatUtils::splitThousands(cr.coverage);
            coveredRegionsText += "<tr>";
            coveredRegionsText += QString("<td align=\"right\">%1&nbsp;&nbsp;</td>").arg(i+1);
            coveredRegionsText += QString("<td><a href=\"%1\">[%2 - %3]</a></td>").arg(i).arg(crStart).arg(crEnd);
            coveredRegionsText += tr("<td align=\"center\">%4</td>").arg(crCoverage);
            coveredRegionsText += "</tr>";
        }
        coveredRegionsText += "</table>";
        text += coveredRegionsText;
    }
    coveredRegionsLabel.setText(text);
    coveredRegionsLabel.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    coveredRegionsLabel.show();

    //p.drawText(rect(), Qt::AlignCenter, );
}

void AssemblyReadsArea::drawReads(QPainter & p) {
    GTIMER(c1, t1, "AssemblyReadsArea::drawReads");
    coveredRegionsLabel.hide();

    p.setFont(browser->getFont());
    p.fillRect(rect(), Qt::white);

    cachedReads.xOffsetInAssembly = browser->getXOffsetInAssembly();
    cachedReads.yOffsetInAssembly = browser->getYOffsetInAssembly();

    cachedReads.visibleBases = U2Region(cachedReads.xOffsetInAssembly, browser->basesCanBeVisible());
    cachedReads.visibleRows = U2Region(cachedReads.yOffsetInAssembly, browser->rowsCanBeVisible());

    // 0. Get reads from the database
    U2OpStatusImpl status;
    qint64 t = GTimer::currentTimeMicros();
    cachedReads.data = model->getReadsFromAssembly(cachedReads.visibleBases, cachedReads.visibleRows.startPos, 
        cachedReads.visibleRows.endPos(), status);
    t = GTimer::currentTimeMicros() - t;
    perfLog.trace(QString("Assembly: reads 2D load time: %1").arg(double(t) / 1000 / 1000));
    if (status.hasError()) {
        LOG_OP(status);
        return;
    }

    // 1. Render cells using AssemblyCellRenderer
    cachedReads.letterWidth = browser->getCellWidth();

    QVector<QImage> cells;
    bool text = browser->areLettersVisible(); 
    if(browser->areCellsVisible()) {
        GTIMER(c3, t3, "AssemblyReadsArea::drawReads -> cells rendering");
        QFont f = browser->getFont();
        if(text) {
            f.setPointSize(calcFontPointSize());
        }
        cells = cellRenderer.render(QSize(cachedReads.letterWidth, cachedReads.letterWidth), text, f);
    }

    // 2. Iterate over all visible reads and draw them
    QListIterator<U2AssemblyRead> it(cachedReads.data);
    while(it.hasNext()) {
        GTIMER(c3, t3, "AssemblyReadsArea::drawReads -> cycle through all reads");

        const U2AssemblyRead & read = it.next();
        QByteArray readSequence = read->readSequence;
        U2Region readBases(read->leftmostPos, U2AssemblyUtils::getEffectiveReadLength(read));

        U2Region readVisibleBases = readBases.intersect(cachedReads.visibleBases);
        U2Region xToDrawRegion(readVisibleBases.startPos - cachedReads.xOffsetInAssembly, readVisibleBases.length);
        if(readVisibleBases.isEmpty()) {
            continue;
        }

        U2Region readVisibleRows = U2Region(read->packedViewRow, 1).intersect(cachedReads.visibleRows); // WTF?
        U2Region yToDrawRegion(readVisibleRows.startPos - cachedReads.yOffsetInAssembly, readVisibleRows.length);
        if(readVisibleRows.isEmpty()) {
            continue;
        }

        if(browser->areCellsVisible()) { //->draw color rects 
            int firstVisibleBase = readVisibleBases.startPos - readBases.startPos; 
            int x_pix_start = browser->calcPainterOffset(xToDrawRegion.startPos);
            int y_pix_start = browser->calcPainterOffset(yToDrawRegion.startPos);

            //iterate over letters of the read
            ShortReadIterator cigarIt(readSequence, read->cigar, firstVisibleBase);
            int basesPainted = 0;
            for(int x_pix_offset = 0; cigarIt.hasNext() && basesPainted++ < readVisibleBases.length; x_pix_offset += cachedReads.letterWidth) {
                GTIMER(c2, t2, "AssemblyReadsArea::drawReads -> cycle through one read");
                char c = cigarIt.nextLetter();
                if(!defaultColorScheme.contains(c)) {
                    //TODO: smarter analysis. Don't forget about '=' symbol and IUPAC codes
                    c = 'N';
                }
                QPoint cellStart(x_pix_start + x_pix_offset, y_pix_start);
                p.drawImage(cellStart, cells[c]);
            }
        } else { 
            int xstart = browser->calcPixelCoord(xToDrawRegion.startPos);
            int xend = browser->calcPixelCoord(xToDrawRegion.endPos());
            int ystart = browser->calcPixelCoord(yToDrawRegion.startPos);
            int yend = browser->calcPixelCoord(yToDrawRegion.endPos());

            p.fillRect(xstart, ystart, xend - xstart, yend - ystart, Qt::black);
        }
    }    
}

void AssemblyReadsArea::drawHint(QPainter & p) {
    if(cachedReads.isEmpty() || cachedReads.letterWidth == 0 || scribbling) {
        sl_hideHint();
        return;
    }

    // 1. find assembly read we stay on
    qint64 asmX = cachedReads.xOffsetInAssembly + (double)curPos.x() / cachedReads.letterWidth;
    qint64 asmY = cachedReads.yOffsetInAssembly + (double)curPos.y() / cachedReads.letterWidth;
    U2AssemblyRead read;
    bool found = false;
    QListIterator<U2AssemblyRead> it(cachedReads.data);
    while(it.hasNext()) {
        const U2AssemblyRead & r = it.next();
        if(r->packedViewRow == asmY && asmX >= r->leftmostPos && asmX < r->leftmostPos + U2AssemblyUtils::getEffectiveReadLength(r)) {
            read = r;
            found = true;
            break;
        }
    }
    if(!found) {
        sl_hideHint();
        return;
    }

    // 2. set hint info
    if(read->id != hintData.curReadId) {
        hintData.curReadId = read->id;
        hintData.hint.setData(read);
    }

    // 3. move hint if needed
    QRect readsAreaRect(mapToGlobal(rect().topLeft()), mapToGlobal(rect().bottomRight()));
    QRect hintRect = hintData.hint.rect(); 
    hintRect.moveTo(QCursor::pos() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR);
    QPoint offset(0, 0);
    if(hintRect.right() > readsAreaRect.right()) {
        offset -= QPoint(hintRect.right() - readsAreaRect.right(), 0);
    }
    if(hintRect.bottom() > readsAreaRect.bottom()) {
        offset -= QPoint(0, hintRect.bottom() - readsAreaRect.bottom()); // move hint bottom to reads area
        offset -= QPoint(0, readsAreaRect.bottom() - QCursor::pos().y() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR.y());
    }
    QPoint newPos = QCursor::pos() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR + offset;
    if(hintData.hint.pos() != newPos) {
        hintData.hint.move(newPos);
    }
    if(!hintData.hint.isVisible()) {
        hintData.hint.show();
    }

    // 4. paint frame around read
    U2Region readBases(read->leftmostPos, U2AssemblyUtils::getEffectiveReadLength(read));
    U2Region readVisibleBases = readBases.intersect(cachedReads.visibleBases);
    assert(!readVisibleBases.isEmpty());
    U2Region xToDrawRegion(readVisibleBases.startPos - cachedReads.xOffsetInAssembly, readVisibleBases.length);

    U2Region readVisibleRows = U2Region(read->packedViewRow, 1).intersect(cachedReads.visibleRows);
    U2Region yToDrawRegion(readVisibleRows.startPos - cachedReads.yOffsetInAssembly, readVisibleRows.length);
    assert(!readVisibleRows.isEmpty());

    assert(browser->areCellsVisible());
    int x_pix_start = browser->calcPainterOffset(xToDrawRegion.startPos);
    int y_pix_start = browser->calcPainterOffset(yToDrawRegion.startPos);

    p.setPen(Qt::darkRed);
    QPoint l(x_pix_start, y_pix_start);
    QPoint r(x_pix_start + readVisibleBases.length * cachedReads.letterWidth, y_pix_start);
    const QPoint off(0, cachedReads.letterWidth);
    p.drawLine(l, r);
    p.drawLine(l + off, r + off);
    if(readBases.startPos == readVisibleBases.startPos) { // draw left border
        p.drawLine(l, l + off);
    }
    if(readBases.endPos() == readVisibleBases.endPos()) { // draw right border
        p.drawLine(r, r + off);
    }
}

int AssemblyReadsArea::calcFontPointSize() const {
    return browser->getCellWidth() / 2;
}

void AssemblyReadsArea::paintEvent(QPaintEvent * e) {
    assert(model);
    drawAll();
    QWidget::paintEvent(e);
}

void AssemblyReadsArea::resizeEvent(QResizeEvent * e) {
    if(e->oldSize().height() - e->size().height()) {
        emit si_heightChanged();
    }
    initRedraw();
    QWidget::resizeEvent(e);
}

void AssemblyReadsArea::wheelEvent(QWheelEvent * e) {
    bool positive = e->delta() > 0;
    int numDegrees = abs(e->delta()) / 8;
    int numSteps = numDegrees / 15;

    // zoom
    if(Qt::NoButton == e->buttons()) {
        for(int i = 0; i < numSteps; ++i) {
            if(positive) {
                browser->sl_zoomIn();
            } else {
                browser->sl_zoomOut();
            }
        }
    }
    QWidget::wheelEvent(e);
}

void AssemblyReadsArea::mousePressEvent(QMouseEvent * e) {
    if(browser->getCellWidth() != 0 && e->button() == Qt::LeftButton) {
        scribbling = true;
        setCursor(Qt::ClosedHandCursor);
        mover = ReadsMover(browser->getCellWidth(), e->pos());
    }
    QWidget::mousePressEvent(e);
}

void AssemblyReadsArea::mouseReleaseEvent(QMouseEvent * e) {
    if(e->button() == Qt::LeftButton && scribbling) {
        scribbling = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mousePressEvent(e);
}

void AssemblyReadsArea::mouseMoveEvent(QMouseEvent * e) {
    emit si_mouseMovedToPos(e->pos());
    if((e->buttons() & Qt::LeftButton) && scribbling) {
        mover.handleEvent(e->pos());
        int x_units = mover.getXunits();
        int y_units = mover.getYunits();
        browser->adjustOffsets(-x_units, -y_units);
    }
    curPos = e->pos();
    hintData.redrawHint = true;
    update();
}

void AssemblyReadsArea::leaveEvent(QEvent * e) {
    QPoint curInHintCoords = hintData.hint.mapFromGlobal(QCursor::pos());
    if(!hintData.hint.rect().contains(curInHintCoords)) {
        sl_hideHint();
    }
}

void AssemblyReadsArea::hideEvent(QHideEvent * e) {
    sl_hideHint();
}

bool AssemblyReadsArea::event(QEvent * e) {
    QEvent::Type t = e->type();
    if(t == QEvent::WindowDeactivate) {
        sl_hideHint();
        hintData.redrawHint = false;
    }
    return QWidget::event(e);
}

void AssemblyReadsArea::keyPressEvent(QKeyEvent * e) {
    int k = e->key();
    if(k == Qt::Key_Left || k == Qt::Key_Right) {
        if(hBar->isEnabled()) {
            int step = e->modifiers() & Qt::ControlModifier ? hBar->pageStep() : hBar->singleStep();
            step = k == Qt::Key_Left ? -step : step;
            hBar->setValue(hBar->value() + step);
            e->accept();
        }
    } else if(k == Qt::Key_Up || k == Qt::Key_Down) {
        if(vBar->isEnabled()) {
            int step = e->modifiers() & Qt::ControlModifier ? vBar->pageStep() : vBar->singleStep();
            step = k == Qt::Key_Up ? -step : step;
            vBar->setValue(vBar->value() + step);
            e->accept();    
        }
    } else if(k == Qt::Key_Home) {
        if(hBar->isEnabled()) {
            hBar->setValue(0);
            e->accept();
        }
    } else if(k == Qt::Key_End) {
        if(hBar->isEnabled()) {
            U2OpStatusImpl status;
            hBar->setValue(model->getModelLength(status));
            LOG_OP(status);
            e->accept();
        }
    } else if(k == Qt::Key_Plus) {
        browser->sl_zoomIn();
        e->accept();
    } else if(k == Qt::Key_Minus) {
        browser->sl_zoomOut();
        e->accept();
    } else if(k == Qt::Key_G && (e->modifiers() & Qt::ControlModifier)) {
        browser->setFocusToPosSelector();
        e->accept();
    } else if(k == Qt::Key_PageUp || k == Qt::Key_PageDown) {
        if(vBar->isEnabled()) {
            int step = k == Qt::Key_PageUp ? -vBar->pageStep() : vBar->pageStep();
            vBar->setValue(vBar->value() + step);
            e->accept();
        }
    }

    if(!e->isAccepted()) {
        QWidget::keyPressEvent(e);
    }
}

void AssemblyReadsArea::mouseDoubleClickEvent(QMouseEvent * e) {
    qint64 cursorXoffset = browser->calcAsmPosX(e->pos().x());
    qint64 cursorYoffset = browser->calcAsmPosY(e->pos().y());

    //1. zoom in
    static const int howManyZoom = 1;
    for(int i = 0; i < howManyZoom; ++i) {
        browser->sl_zoomIn();
    }

    //2. move cursor offset to center
    // move x
    if(hBar->isEnabled()) {
        qint64 xOffset = browser->getXOffsetInAssembly();
        qint64 windowHalfX = xOffset + qRound64((double)browser->basesCanBeVisible() / 2);
        browser->setXOffsetInAssembly(browser->normalizeXoffset(xOffset + cursorXoffset - windowHalfX + 1));
    }
    // move y
    if(vBar->isEnabled()) {
        qint64 yOffset = browser->getYOffsetInAssembly();
        qint64 windowHalfY = yOffset + qRound64((double)browser->rowsCanBeVisible() / 2);
        browser->setYOffsetInAssembly(browser->normalizeYoffset(yOffset + cursorYoffset - windowHalfY + 1));
    }
}

void AssemblyReadsArea::sl_coveredRegionClicked(const QString & link) {
    bool ok;
    int i = link.toInt(&ok);
    assert(ok);
    CoveredRegion cr = browser->getCoveredRegions().at(i);
    ui->getOverview()->checkedSetVisibleRange(cr.region.startPos, cr.region.length);
}

void AssemblyReadsArea::sl_onHScrollMoved(int pos) {
    browser->setXOffsetInAssembly(pos);
}

void AssemblyReadsArea::sl_onVScrollMoved(int pos) {
    browser->setYOffsetInAssembly(pos);
}

void AssemblyReadsArea::sl_zoomOperationPerformed() {
    sl_redraw();
}

void AssemblyReadsArea::sl_redraw() {
    initRedraw();
    update();
}

void AssemblyReadsArea::sl_hideHint() {
    hintData.hint.hide();
    update();

}


} //ns
