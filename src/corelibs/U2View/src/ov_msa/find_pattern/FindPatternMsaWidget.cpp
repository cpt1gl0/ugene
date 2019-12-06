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

#include <QFlags>
#include <QKeyEvent>
#include <QMovie>
#include <QMessageBox>

#include <U2Algorithm/FindAlgorithmTask.h>

#include <U2Core/AnnotationTableObject.h>
#include <U2Core/AppContext.h>
#include <U2Core/Counter.h>
#include <U2Core/CreateAnnotationTask.h>
#include <U2Core/DNAAlphabet.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/DNASequenceSelection.h>
#include <U2Core/DNATranslation.h>
#include <U2Core/DocumentUtils.h>
#include <U2Core/GenbankFeatures.h>
#include <U2Core/L10n.h>
#include <U2Core/Log.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/TextUtils.h>
#include <U2Core/U1AnnotationUtils.h>
#include <U2Core/U2DbiRegistry.h>
#include <U2Core/U2DbiUtils.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Formats/FastaFormat.h>

#include <U2Gui/CreateAnnotationWidgetController.h>
#include <U2Gui/DialogUtils.h>
#include <U2Gui/LastUsedDirHelper.h>
#include <U2Gui/ShowHideSubgroupWidget.h>
#include <U2Gui/U2FileDialog.h>
#include <U2Gui/U2WidgetStateStorage.h>
#include <U2Gui/GUIUtils.h>

#include <U2View/ADVSequenceObjectContext.h>
#include <U2View/ADVSequenceWidget.h>
#include <U2View/ADVSingleSequenceWidget.h>
#include <U2View/AnnotatedDNAView.h>
#include <U2View/DetView.h>

#include <U2Core/U2AlphabetUtils.h>

#include "FindPatternMsaTask.h"
#include "FindPatternMsaWidget.h"

#include "../MSAEditorSequenceArea.h"

const QString NEW_LINE_SYMBOL = "\n";
const QString STYLESHEET_COLOR_DEFINITION = "color: ";
const QString STYLESHEET_DEFINITIONS_SEPARATOR = ";";

const int REG_EXP_MIN_RESULT_LEN = 1;
const int REG_EXP_MAX_RESULT_LEN = 1000;
const int REG_EXP_MAX_RESULT_SINGLE_STEP = 20;

namespace U2 {

const static QString SHOW_OPTIONS_LINK("show_options_link");

const int FindPatternMsaWidget::DEFAULT_RESULTS_NUM_LIMIT = 100000;
const int FindPatternMsaWidget::DEFAULT_REGEXP_RESULT_LENGTH_LIMIT = 10000;

class PatternWalker {
public:
    PatternWalker(const QString& _patternsString, int _cursor = 0)
        : patternsString(_patternsString.toLatin1()), cursor(_cursor)
    {
        current = -1;
    }

    bool hasNext() const {
        return (current < patternsString.size() - 1);
    }

    char next() {
        if (!hasNext()) {
            return 0;
        }
        current++;
        return patternsString[current];
    }

    bool isSequenceChar() const {
        CHECK(-1 != current, false);
        CHECK(current < patternsString.size(), false);
        return true;
    }

    /** moves current place to the previous */
    void removeCurrent() {
        CHECK(-1 != current, );
        CHECK(current < patternsString.size(), );
        patternsString.remove(current, 1);
        if (current < cursor) {
            cursor--;
        }
        current--;
    }

    bool isCorrect() const {
        if (!isSequenceChar()) {
            return true;
        }
        QChar c(patternsString[current]);
        if (c.isLetter()) {
            return c.isUpper();
        }
        else {
            return ('\n' == c);
        }
    }

    void setCurrent(char value) {
        CHECK(-1 != current, );
        CHECK(current < patternsString.size(), );
        patternsString[current] = value;
    }

    int getCursor() const {
        return cursor;
    }

    QString getString() const {
        return patternsString;
    }

private:
    QByteArray patternsString;
    int cursor;
    int current;
};


FindPatternMsaWidget::FindPatternMsaWidget(MSAEditor* _msaEditor) :
    msaEditor(_msaEditor),
    searchTask(NULL),
    previousMaxResult(-1),
    savableWidget(this, GObjectViewUtils::findViewByName(msaEditor->getName()))
{
    setupUi(this);
    progressMovie = new QMovie(":/core/images/progress.gif", QByteArray(), progressLabel);
    progressLabel->setObjectName("progressLabel");
    resultLabel->setObjectName("resultLabel");
    resultLabel->setFixedHeight(progressLabel->height());
    savableWidget.setRegionWidgetIds(QStringList() << editStart->objectName()
        << editEnd->objectName());
    progressLabel->setMovie(progressMovie);
    
    setContentsMargins(0, 0, 0, 0);

    const DNAAlphabet* alphabet = msaEditor->getMaObject()->getAlphabet();
    isAminoSequenceSelected = alphabet->isAmino();

    initLayout();
    connectSlots();

    checkState();
    if (lblErrorMessage->text().isEmpty()) {
        showHideMessage(true, UseMultiplePatternsTip);
    }


    FindPatternEventFilter *findPatternEventFilter = new FindPatternEventFilter(this);
    textPattern->installEventFilter(findPatternEventFilter);

    setFocusProxy(textPattern);

    connect(findPatternEventFilter, SIGNAL(si_enterPressed()), SLOT(sl_onEnterPressed()));
    connect(findPatternEventFilter, SIGNAL(si_shiftEnterPressed()), SLOT(sl_onShiftEnterPressed()));

    sl_onSearchPatternChanged();

    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);
    showCurrentResultAndStopProgress(0, 0);
    setUpTabOrder();
    previousMaxResult = boxMaxResult->value();
    U2WidgetStateStorage::restoreWidgetState(savableWidget);
}

void FindPatternMsaWidget::showCurrentResultAndStopProgress(const int current, const int total) {
    progressMovie->stop();
    progressLabel->hide();
    resultLabel->show();
    assert(total >= current);
    resultLabel->setText(tr("Results: %1/%2").arg(QString::number(current)).arg(QString::number(total)));
}

FindPatternMsaWidget::ResultIterator::ResultIterator()
    : results(QMap<int, QList<U2Region> > ()), totalResultsCounter(0), globalPos(0), row(0), pos(0) {}

FindPatternMsaWidget::ResultIterator::ResultIterator(const QMap<int, QList<U2Region> >& results_) 
    : results(results_), totalResultsCounter(0), globalPos(1), row(results.firstKey()), pos(0)
{
    foreach(int key, results.keys()) {
        totalResultsCounter += results[key].size();
    }
}

bool FindPatternMsaWidget::ResultIterator::goNextResult() {
    if (!isNextResult()) {
        return false;
    }
    pos++;
    globalPos++;
    if (pos == results[row].size()) {
        pos = 0;
        row = findNextRow();
    }
    return true;
}

bool FindPatternMsaWidget::ResultIterator::goPrevResult() {
    if (!isPrevResult()) {
        return false;
    }
    if (pos == 0) {
        row = findPrevRow();
        pos = results[row].size() - 1;
    } else {
        pos--;
    }
    globalPos--;
    return true;
}

int FindPatternMsaWidget::ResultIterator::findNextRow() const {
    foreach(int key, results.keys()) {
        if (key > row) {
            return key;
        }
    }
    return -1;
}

int FindPatternMsaWidget::ResultIterator::findPrevRow() const {
    QList<int> keyList(results.keys());
    std::reverse(keyList.begin(), keyList.end());
    foreach(int key, keyList) {
        if (key < row) {
            return key;
        }
    }
    return -1;
}

void FindPatternMsaWidget::initLayout() {
    lblErrorMessage->setStyleSheet("font: bold;");
    lblErrorMessage->setText("");
    initAlgorithmLayout();
    initRegionSelection();
    initResultsLimit();

    subgroupsLayout->setSpacing(0);
    subgroupsLayout->addWidget(new ShowHideSubgroupWidget(QObject::tr("Search algorithm"), QObject::tr("Search algorithm"), widgetAlgorithm, false));
    subgroupsLayout->addWidget(new ShowHideSubgroupWidget(QObject::tr("Search in"), QObject::tr("Search in"), widgetSearchIn, false));
    subgroupsLayout->addWidget(new ShowHideSubgroupWidget(QObject::tr("Other settings"), QObject::tr("Other settings"), widgetOther, false));

    updateLayout();

    layoutSearchButton->setAlignment(Qt::AlignTop);
    this->layout()->setAlignment(Qt::AlignTop);

    this->layout()->setMargin(0);
}


void FindPatternMsaWidget::initAlgorithmLayout()
{
    boxAlgorithm->addItem(tr("Exact"), FindAlgorithmPatternSettings_Exact);
    if(!isAminoSequenceSelected) {
        boxAlgorithm->addItem(tr("InsDel"), FindAlgorithmPatternSettings_InsDel);
        boxAlgorithm->addItem(tr("Substitute"), FindAlgorithmPatternSettings_Subst);
    }
    boxAlgorithm->addItem(tr("Regular expression"), FindAlgorithmPatternSettings_RegExp);

    layoutMismatch = new QHBoxLayout();

    lblMatch = new QLabel(tr("Should match"));

    spinMatch = new QSpinBox(this);
    spinMatch->setSuffix("%"); // Percentage value
    spinMatch->setMinimum(30);
    spinMatch->setMaximum(100);
    spinMatch->setSingleStep(1);
    spinMatch->setValue(100);
    spinMatch->setObjectName("spinBoxMatch");
    spinMatch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layoutMismatch->addWidget(lblMatch);
    layoutMismatch->addWidget(spinMatch);

    layoutAlgorithmSettings->addLayout(layoutMismatch);
    initMaxResultLenContainer();

    selectedAlgorithm = boxAlgorithm->itemData(boxAlgorithm->currentIndex()).toInt();
}

void FindPatternMsaWidget::initRegionSelection()
{
    boxRegion->addItem(FindPatternMsaWidget::tr("Whole sequence"), RegionSelectionIndex_WholeSequence);
    boxRegion->addItem(FindPatternMsaWidget::tr("Custom region"), RegionSelectionIndex_CustomRegion);
    boxRegion->addItem(FindPatternMsaWidget::tr("Selected region"), RegionSelectionIndex_CurrentSelectedRegion);
    /*
    ADVSequenceObjectContext* activeContext = annotatedDnaView->getSequenceInFocus();
    SAFE_POINT(NULL != activeContext, "Internal error: sequence context is NULL during region selection init.",);

    setRegionToWholeSequence();

    editStart->setValidator(new QIntValidator(1, activeContext->getSequenceLength(), editStart));
    editEnd->setValidator(new QIntValidator(1, activeContext->getSequenceLength(), editEnd));

    currentSelection = annotatedDnaView->getSequenceInFocus()->getSequenceSelection();

    sl_onRegionOptionChanged(RegionSelectionIndex_WholeSequence);
    */
}


void FindPatternMsaWidget::initResultsLimit()
{
    boxMaxResult->setMinimum(1);
    boxMaxResult->setMaximum(INT_MAX);
    boxMaxResult->setValue(DEFAULT_RESULTS_NUM_LIMIT);
    boxMaxResult->setEnabled(true);
}

void FindPatternMsaWidget::initMaxResultLenContainer() {
    useMaxResultLenContainer = new QWidget();

    layoutRegExpLen = new QVBoxLayout();
    layoutRegExpLen->setContentsMargins(0, 0, 0, 0);
    layoutRegExpLen->setSpacing(3);
    layoutRegExpLen->setSizeConstraint(QLayout::SetMinAndMaxSize);
    useMaxResultLenContainer->setLayout(layoutRegExpLen);

    QHBoxLayout *layoutUseMaxResultLen = new QHBoxLayout();
    layoutUseMaxResultLen->setSpacing(10);
    layoutUseMaxResultLen->setSizeConstraint(QLayout::SetMinAndMaxSize);

    boxUseMaxResultLen = new QCheckBox();
    boxUseMaxResultLen->setObjectName("boxUseMaxResultLen");
    QLabel *labelUseMaxResultLen = new QLabel(tr("Results no longer than:"));
    labelUseMaxResultLen->setWordWrap(true);
    layoutUseMaxResultLen->addWidget(boxUseMaxResultLen, 0);
    layoutUseMaxResultLen->addWidget(labelUseMaxResultLen, 1);

    boxMaxResultLen = new QSpinBox();
    boxMaxResultLen->setObjectName("boxMaxResultLen");
    boxMaxResultLen->setMinimum(REG_EXP_MIN_RESULT_LEN);
    boxMaxResultLen->setMaximum(REG_EXP_MAX_RESULT_LEN);
    boxMaxResultLen->setSingleStep(REG_EXP_MAX_RESULT_SINGLE_STEP);
    boxMaxResultLen->setValue(REG_EXP_MAX_RESULT_LEN);
    boxMaxResultLen->setEnabled(false);
    connect(boxUseMaxResultLen, SIGNAL(toggled(bool)), boxMaxResultLen, SLOT(setEnabled(bool)));
    connect(boxUseMaxResultLen, SIGNAL(toggled(bool)), SLOT(sl_activateNewSearch()));
    connect(boxMaxResultLen, SIGNAL(valueChanged(int)), SLOT(sl_activateNewSearch()));

    layoutRegExpLen->addLayout(layoutUseMaxResultLen);
    layoutRegExpLen->addWidget(boxMaxResultLen);
    layoutAlgorithmSettings->addWidget(useMaxResultLenContainer);
}


void FindPatternMsaWidget::connectSlots()
{
    connect(boxAlgorithm, SIGNAL(currentIndexChanged(int)), SLOT(FindPatternMsaWidget::sl_onAlgorithmChanged(int)));
    connect(boxRegion, SIGNAL(currentIndexChanged(int)), SLOT(sl_onRegionOptionChanged(int)));
    connect(textPattern, SIGNAL(textChanged()), SLOT(sl_onSearchPatternChanged()));
    connect(editStart, SIGNAL(textChanged(QString)), SLOT(sl_onRegionValueEdited()));
    connect(editEnd, SIGNAL(textChanged(QString)), SLOT(sl_onRegionValueEdited()));
    connect(boxMaxResult, SIGNAL(valueChanged(int)), SLOT(sl_onMaxResultChanged(int)));
    connect(removeOverlapsBox, SIGNAL(stateChanged(int)), SLOT(sl_activateNewSearch()));

    // A sequence has been selected in the Sequence View
    /*
    connect(annotatedDnaView, SIGNAL(si_focusChanged(ADVSequenceWidget*, ADVSequenceWidget*)),
        this, SLOT(sl_onFocusChanged(ADVSequenceWidget*, ADVSequenceWidget*)));
    */
    // A sequence has been modified (a subsequence added, removed, etc.)
    /*
    connect(annotatedDnaView->getSequenceInFocus()->getSequenceObject(), SIGNAL(si_sequenceChanged()),
        this, SLOT(sl_onSequenceModified()));
    */
    connect(prevPushButton, SIGNAL(clicked()), SLOT(sl_prevButtonClicked()));
    connect(nextPushButton, SIGNAL(clicked()), SLOT(sl_nextButtonClicked()));
    connect(spinMatch, SIGNAL(valueChanged(int)), SLOT(sl_activateNewSearch()));
}

void FindPatternMsaWidget::sl_nigguz() {

}

void FindPatternMsaWidget::sl_onAlgorithmChanged(int index)
{
    int previousAlgorithm = selectedAlgorithm;
    selectedAlgorithm = boxAlgorithm->itemData(index).toInt();
    updatePatternText(previousAlgorithm);
    updateLayout();
    bool noValidationErrors = verifyPatternAlphabet();
    if (noValidationErrors) {
        sl_activateNewSearch(true);
    }
}


void FindPatternMsaWidget::sl_onRegionOptionChanged(int index)
{
    /*
    if (!currentSelection.isEmpty()){
        disconnect(currentSelection, SIGNAL(si_selectionChanged(LRegionsSelection*, const QVector<U2Region>&, const QVector<U2Region>&)),
            this, SLOT(sl_onSelectedRegionChanged()));
    }
    if (boxRegion->itemData(index).toInt() == RegionSelectionIndex_WholeSequence) {
        editStart->hide();
        lblStartEndConnection->hide();
        editEnd->hide();
        regionIsCorrect = true;
        checkState();
        setRegionToWholeSequence();
    }else if (boxRegion->itemData(index).toInt() == RegionSelectionIndex_CustomRegion) {
        editStart->show();
        lblStartEndConnection->show();
        editEnd->show();
        editStart->setReadOnly(false);
        editEnd->setReadOnly(false);

        ADVSequenceObjectContext* activeContext = annotatedDnaView->getSequenceInFocus();
        SAFE_POINT(NULL != activeContext, "Internal error: there is no sequence in focus!",);
        getCompleteSearchRegion(regionIsCorrect, activeContext->getSequenceLength());
        checkState();
    }else if(boxRegion->itemData(index).toInt() == RegionSelectionIndex_CurrentSelectedRegion) {
        currentSelection = annotatedDnaView->getSequenceInFocus()->getSequenceSelection();
        connect(currentSelection, SIGNAL(si_selectionChanged(LRegionsSelection* , const QVector<U2Region>&, const QVector<U2Region>&)),
            this, SLOT(sl_onSelectedRegionChanged()) );
        editStart->show();
        lblStartEndConnection->show();
        editEnd->show();
        sl_onSelectedRegionChanged();
    }
    */
}

void FindPatternMsaWidget::sl_onRegionValueEdited()
{
    regionIsCorrect = true;

    // The values are not empty
    if (editStart->text().isEmpty()) {
        TextUtils::highlightBackground(editStart);
        regionIsCorrect = false;
    }
    else if (editEnd->text().isEmpty()) {
        TextUtils::highlightBackground(editEnd);
        regionIsCorrect = false;
    } else {
        bool ok = false;
        qint64 value1 = editStart->text().toLongLong(&ok);
        if (!ok || (value1 < 1)) {
            TextUtils::highlightBackground(editStart);
            regionIsCorrect = false;
        }
        int value2 = editEnd->text().toLongLong(&ok);
        if (!ok || value2 < 1) {
            TextUtils::highlightBackground(editEnd);
            regionIsCorrect = false;
        }
    }

    if (regionIsCorrect) {
        TextUtils::doNotHighlightBackground(editStart);
        TextUtils::doNotHighlightBackground(editEnd);
    }

    boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));

    checkState();
    if(regionIsCorrect){
        sl_activateNewSearch();
    }
}


void FindPatternMsaWidget::sl_onFocusChanged(
    MSAEditor* /* prevWidget */, MSAEditor* currentWidget)
{
    Q_UNUSED(currentWidget);
    /*
    ADVSequenceObjectContext* activeContext = annotatedDnaView->getSequenceInFocus();
    if (activeContext != 0) {
        const DNAAlphabet* alphabet = activeContext->getAlphabet();
        isAminoSequenceSelected = alphabet->isAmino();
        updateLayout();

        // Update region
        setRegionToWholeSequence();
        doNotHighlightBackground(editStart);
        doNotHighlightBackground(editEnd);

        // Update available annotations table objects, etc.
//         if(boxRegion->itemData(boxRegion->currentIndex()).toInt() == RegionSelectionIndex_CurrentSelectedRegion){
//             disconnect(currentSelection, SIGNAL(si_selectionChanged(LRegionsSelection* , const QVector<U2Region>&, const QVector<U2Region>&)),
//                 this, SLOT(sl_onSelectedRegionChanged()) );
//             currentSelection = annotatedDnaView->getSequenceInFocus()->getSequenceSelection();
//             connect(currentSelection, SIGNAL(si_selectionChanged(LRegionsSelection* , const QVector<U2Region>&, const QVector<U2Region>&)),
//                 this, SLOT(sl_onSelectedRegionChanged()) );
//              sl_onSelectedRegionChanged();
//         }
    }
*/
}


void FindPatternMsaWidget::updateLayout()
{
    // Algorithm group
    if (selectedAlgorithm == FindAlgorithmPatternSettings_Exact) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        spinMatch->hide();
        lblMatch->hide();
    }
    if (selectedAlgorithm == FindAlgorithmPatternSettings_InsDel) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        enableDisableMatchSpin();
        lblMatch->show();
        spinMatch->show();
        QWidget::setTabOrder(boxAlgorithm, spinMatch);
    }
    else if (selectedAlgorithm == FindAlgorithmPatternSettings_Subst) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        QWidget::setTabOrder(boxAlgorithm, spinMatch);
        enableDisableMatchSpin();
        lblMatch->show();
        spinMatch->show();
    }
    else if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        useMaxResultLenContainer->show();
        boxMaxResultLen->show();
        spinMatch->hide();
        lblMatch->hide();
        QWidget::setTabOrder(boxAlgorithm, boxUseMaxResultLen);
        QWidget::setTabOrder(boxUseMaxResultLen, boxMaxResultLen);
    }
}

void FindPatternMsaWidget::showHideMessage( bool show, MessageFlag messageFlag, const QString& additionalMsg ){
    if (show) {
        if (!messageFlags.contains(messageFlag)) {
            messageFlags.append(messageFlag);
        }
    }
    else {
        messageFlags.removeAll(messageFlag);
    }

    if (!messageFlags.isEmpty()) {

#ifndef Q_OS_MAC
        const QString lineBreakShortcut = "Ctrl+Enter";
#else
        const QString lineBreakShortcut = "Cmd+Enter";
#endif
        QString text = "";
        foreach (MessageFlag flag, messageFlags) {
            switch (flag) {
                case PatternIsTooLong:
                    {
                    const QString message = tr("The value is longer than the search region."
                                               " Please input a shorter value or select another region!");
                    text = tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    break;
                    }
                case PatternAlphabetDoNotMatch:
                    {
                    const QString message = tr("Warning: input value contains characters that"
                                               " do not match the active alphabet!");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::warningColorLabelHtmlStr()).arg(message);
                    TextUtils::highlightBackground(textPattern);
                    break;
                    }
                case PatternsWithBadAlphabetInFile:
                    {
                    const QString message = tr("Warning: file contains patterns that"
                                               " do not match the active alphabet! Those patterns were ignored ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::warningColorLabelHtmlStr()).arg(message);
                    break;
                    }
                case PatternsWithBadRegionInFile:
                    {
                    const QString message = tr("Warning: file contains patterns that"
                                               " longer than the search region! Those patterns were ignored. Please input a shorter value or select another region! ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::warningColorLabelHtmlStr()).arg(message);
                    break;
                    }
                case UseMultiplePatternsTip:
                    {
                    const QString message = tr("Info: please input at least one sequence pattern to search for.").arg(lineBreakShortcut);
                    text = tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::infoColorLabelHtmlStr()).arg(message);
                    break;
                    }
                case AnnotationNotValidName:
                    {
                    const QString message = tr("Warning: annotation name or annotation group name are invalid. ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    if (!additionalMsg.isEmpty()){
                        const QString message = tr("Reason: ");
                        text += tr("<b><font color=%1>%2</font></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                        text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(additionalMsg);
                    }
                    const QString msg = tr(" Please input valid annotation names. ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(msg);
                    break;
                    }
                case AnnotationNotValidFastaParsedName:
                    {
                    const QString message = tr("Warning: annotation names are invalid. ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    if (!additionalMsg.isEmpty()){
                        const QString message = tr("Reason: ");
                        text += tr("<b><font color=%1>%2</font></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                        text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(additionalMsg);
                    }
                    const QString msg = tr(" It will be automatically changed to acceptable name if 'Get annotations' button is pressed. ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(msg);
                    break;
                    }
                case NoPatternToSearch:
                    {
                    const QString message = tr("Warning: there is no pattern to search. ");
                    text += tr("<b><font color=%1>%2</font></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    const QString msg = tr(" Please input a valid pattern or choose a file with patterns ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(msg);
                    break;
                    }
                case SearchRegionIncorrect:
                    {
                    const QString message = tr("Warning: there is no pattern to search. ");
                    text += tr("<b><font color=%1>%2</font></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    const QString msg = tr(" Please input a valid pattern or choose a file with patterns ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(msg);
                    break;
                    }
                case PatternWrongRegExp:
                    {
                    const QString message = tr("Warning: invalid regexp. ");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    TextUtils::highlightBackground(textPattern);
                    break;
                    }
                case SequenceIsTooBig:
                    {
                    text.clear(); // the search is blocked at all -- any other messages are meaningless
                    const QString message = tr("Warning: current sequence is too long to search in.");
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(L10N::errorColorLabelHtmlStr()).arg(message);
                    break;
                    }
                default:
                    FAIL("Unexpected value of the error flag in show/hide error message for pattern!",);
            }
        }
        lblErrorMessage->setText(text);
    }
    else {
        lblErrorMessage->setText("");
    }
    bool hasNoErrors = messageFlags.isEmpty() || (messageFlags.size() == 1 && messageFlags.contains(UseMultiplePatternsTip));
    if (hasNoErrors) {
        TextUtils::doNotHighlightBackground(textPattern);
    }

}

void FindPatternMsaWidget::sl_onSearchPatternChanged()
{
    static QString patterns = "";
    if (patterns != textPattern->toPlainText()) {
        patterns = textPattern->toPlainText();
        setCorrectPatternsString();
        checkState();
        if (lblErrorMessage->text().isEmpty()) {
            showHideMessage(patterns.isEmpty(), UseMultiplePatternsTip);
        }

        enableDisableMatchSpin();

        // Show a warning if the pattern alphabet doesn't match,
        // but do not block the "Search" button
        bool noValidationErrors = verifyPatternAlphabet();
        if (noValidationErrors && patterns != previousPatternString) {
            previousPatternString = patterns;
            sl_activateNewSearch(false);
        }
    }
}

void FindPatternMsaWidget::sl_onMaxResultChanged(int newMaxResult) {
    /*
    bool limitResult = !findPatternResults.isEmpty() && newMaxResult < findPatternResults.size();
    bool widenResult = newMaxResult > previousMaxResult && findPatternResults.size() == previousMaxResult;
    bool prevSearchIsNotComplete = findPatternResults.isEmpty() && searchTask != NULL;
    if (limitResult || widenResult || prevSearchIsNotComplete) {
        sl_activateNewSearch();
    }
    */
}

void FindPatternMsaWidget::setCorrectPatternsString() {
    QTextCursor cursorInTextEdit = textPattern->textCursor();

    if (FindAlgorithmPatternSettings_RegExp != selectedAlgorithm) {
        PatternWalker walker(textPattern->toPlainText(), cursorInTextEdit.position());
        // Delete all non-alphabet symbols.
        while (walker.hasNext()) {
            QChar character(walker.next());
            if (walker.isCorrect()) {
                continue;
            }
            if (character.isLetter()) {
                if (!character.isUpper()) {
                    walker.setCurrent(character.toUpper().toLatin1());
                }
            }
            else {
                if ('\n' != character) {
                    walker.removeCurrent();
                }
            }
        }

        if (textPattern->toPlainText() != walker.getString()) {
            textPattern->setText(walker.getString());
            cursorInTextEdit.setPosition(walker.getCursor());
            textPattern->setTextCursor(cursorInTextEdit);
        }
    }
}

void FindPatternMsaWidget::setRegionToWholeSequence()
{
    editStart->setText(QString::number(1));
    editEnd->setText(QString::number(msaEditor->getAlignmentLen()));
    regionIsCorrect = true;
    boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_WholeSequence));
}

bool FindPatternMsaWidget::verifyPatternAlphabet()
{
    bool alphabetIsOk = checkAlphabet(textPattern->toPlainText());
    if (!alphabetIsOk) {
        showHideMessage(true, PatternAlphabetDoNotMatch);
    }
    else {
        showHideMessage(false, PatternAlphabetDoNotMatch);
    }

    bool result = alphabetIsOk;

    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        QRegExp regExp(textPattern->toPlainText());
        if (regExp.isValid()) {
            showHideMessage(false, PatternWrongRegExp);
        } else {
            showHideMessage(true, PatternWrongRegExp);
            result = false;
        }
    } else {
        showHideMessage(false, PatternWrongRegExp);
    }
    return result;
}

void FindPatternMsaWidget::sl_onSequenceTranslationChanged(int /* index */)
{
    verifyPatternAlphabet();
}

void FindPatternMsaWidget::sl_onSequenceModified()
{
    setRegionToWholeSequence();
    checkState();
    verifyPatternAlphabet();
}

void FindPatternMsaWidget::showTooLongSequenceError()
{
    showHideMessage(true, SequenceIsTooBig);

    showHideMessage(false, AnnotationNotValidFastaParsedName);
    showHideMessage(false, AnnotationNotValidName);
    showHideMessage(false, PatternAlphabetDoNotMatch);
    showHideMessage(false, PatternsWithBadRegionInFile);
    showHideMessage(false, PatternsWithBadAlphabetInFile);
    showHideMessage(false, NoPatternToSearch);
    showHideMessage(false, SearchRegionIncorrect);
    TextUtils::doNotHighlightBackground(textPattern);
}

void FindPatternMsaWidget::checkState()
{
    // Disable the "Search" button if the pattern is empty
    //and pattern is not loaded from a file
    if (textPattern->toPlainText().isEmpty()) {
        showHideMessage(false, PatternAlphabetDoNotMatch);
        TextUtils::doNotHighlightBackground(textPattern);
        return;
    }

    // Show warning if the region is not correct
    if (!regionIsCorrect) {
        showHideMessage(true, SearchRegionIncorrect);
        return;
    }
    
    // Show warning if the length of the pattern is greater than the search region length
    // Not for RegExp algorithm
    if (FindAlgorithmPatternSettings_RegExp != selectedAlgorithm) {
        bool regionOk = checkPatternRegion(textPattern->toPlainText());

        if (!regionOk) {
            TextUtils::highlightBackground(textPattern);
            showHideMessage(true, PatternIsTooLong);
            return;
        }
        else {
            TextUtils::doNotHighlightBackground(textPattern);
            showHideMessage(false, PatternIsTooLong);
        }
    }    

    showHideMessage(false, AnnotationNotValidFastaParsedName);
    showHideMessage(false, AnnotationNotValidName);
    showHideMessage(false, PatternsWithBadRegionInFile);
    showHideMessage(false, PatternsWithBadAlphabetInFile);
    showHideMessage(false, NoPatternToSearch);
    showHideMessage(false, SearchRegionIncorrect);
    showHideMessage(false, SequenceIsTooBig);
}


void FindPatternMsaWidget::enableDisableMatchSpin()
{
    if (textPattern->toPlainText().isEmpty() || isAminoSequenceSelected) {
        spinMatch->setEnabled(false);
    }
    else {
        spinMatch->setEnabled(true);
    }
}

U2Region FindPatternMsaWidget::getCompleteSearchRegion(bool& regionIsCorrect, qint64 maxLen) const
{
    if (boxRegion->itemData(boxRegion->currentIndex()).toInt() == RegionSelectionIndex_WholeSequence) {
        regionIsCorrect = true;
        return U2Region(0, maxLen);
    }
    bool ok = false;
    qint64 value1 = editStart->text().toLongLong(&ok) - 1;
    if (!ok || value1 < 0) {
        regionIsCorrect = false;
        return U2Region();
    }

    int value2 = editEnd->text().toLongLong(&ok);
    if (!ok || value2 <= 0 || value2 > maxLen){
        regionIsCorrect = false;
        return U2Region();
    }

    if (value1 > value2 ) { // start > end
        value2 += maxLen;
    }

    regionIsCorrect = true;
    return U2Region(value1, value2 - value1);
}

int FindPatternMsaWidget::getMaxError( const QString& pattern ) const{
    if (selectedAlgorithm == FindAlgorithmPatternSettings_Exact) {
        return 0;
    }
    return int((float)(1 - float(spinMatch->value()) / 100) * pattern.length());
}

QList <QPair<QString, QString> > FindPatternMsaWidget::getPatternsFromTextPatternField(U2OpStatus& os) const {
    QString inputText = textPattern->toPlainText().toLocal8Bit();
    QList <NamePattern > result = FastaFormat::getSequencesAndNamesFromUserInput(inputText, os);

    if (result.isEmpty()) {
        QStringList patterns = inputText.split(QRegExp("\n"), QString::SkipEmptyParts);
        foreach(const QString & pattern, patterns) {
            result.append(qMakePair(QString(""), pattern));
        }
    }
    return result;
}

#define FIND_PATTER_LAST_DIR "Find_pattern_last_dir"

void FindPatternMsaWidget::initFindPatternTask(const QList<NamePattern> &patterns) {
    CHECK(!patterns.isEmpty(), );

    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        QRegExp regExp(textPattern->toPlainText());
        CHECK(regExp.isValid(), );
    }

    FindPatternMsaSettings settings;
    settings.patterns = patterns;
    settings.msaObj = msaEditor->getMaObject();
    U2OpStatusImpl os;
    CHECK_OP_EXT(os, showTooLongSequenceError(), ); // suppose that if the sequence cannot be fetched from the DB, UGENE ran out of memory

    // Limit results number to the specified value
    settings.findSettings.maxResult2Find =  boxMaxResult->value();
    previousMaxResult = settings.findSettings.maxResult2Find;

    // Region
    bool regionIsCorrectRef = false;
    U2Region region = getCompleteSearchRegion(regionIsCorrectRef, msaEditor->getAlignmentLen());
    SAFE_POINT(true == regionIsCorrectRef, "Internal error: incorrect search region has been supplied."
        " Skipping the pattern search.", );
    settings.findSettings.searchRegion = region;

    // Algorithm settings
    settings.findSettings.patternSettings = static_cast<FindAlgorithmPatternSettings>(selectedAlgorithm);

    settings.findSettings.maxErr = 0;

    settings.findSettings.maxRegExpResult = boxUseMaxResultLen->isChecked() ?
        boxMaxResultLen->value() :
    DEFAULT_REGEXP_RESULT_LENGTH_LIMIT;

    // Creating and registering the task
    bool removeOverlaps = removeOverlapsBox->isChecked();

    SAFE_POINT(searchTask == NULL, "Search task is not NULL", );
    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);

    searchTask = new FindPatternMsaTask(settings);
    connect(searchTask, SIGNAL(si_stateChanged()), SLOT(sl_findPatrernTaskStateChanged()));
    startProgressAnimation();
    AppContext::getTaskScheduler()->registerTopLevelTask(searchTask);
}

void FindPatternMsaWidget::sl_findPatrernTaskStateChanged() {
    FindPatternMsaTask *findTask = static_cast<FindPatternMsaTask*>(sender());
    CHECK(NULL != findTask, );
    if (findTask != searchTask){
        return;
    }
    if (findTask->isFinished() || findTask->isCanceled() || findTask->hasError()) {
        findPatternResults = findTask->getResults();
        if (findPatternResults.isEmpty()) {
            showCurrentResultAndStopProgress(0, 0);
            nextPushButton->setDisabled(true);
            prevPushButton->setDisabled(true);
        } else {
            resultIterator = ResultIterator(findPatternResults);
            //qSort(findPatternResults.begin(), findPatternResults.end(), compareByRegionStartPos);
            showCurrentResultAndStopProgress(resultIterator.getGlobalPos(), resultIterator.getTotalCount());
            nextPushButton->setEnabled(true);
            prevPushButton->setEnabled(true);
            checkState();
            correctSearchInCombo();
            showCurrentResult();
        }
        searchTask = NULL;
    }
}

bool FindPatternMsaWidget::checkAlphabet( const QString& pattern ){
    const DNAAlphabet* alphabet = msaEditor->getMaObject()->getAlphabet();
    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        return true;
    }
    bool patternFitsIntoAlphabet = TextUtils::fits(alphabet->getMap(), pattern.toLocal8Bit().data(), pattern.size());
    if (patternFitsIntoAlphabet) {
        return true;
    }
    return false;
}

bool FindPatternMsaWidget::checkPatternRegion( const QString& pattern ){
    int maxError = getMaxError(pattern);
    qint64 patternLength = pattern.length();
    qint64 minMatch = patternLength - maxError;
    SAFE_POINT(minMatch > 0, "Search pattern length is greater than max error value!",false);

    bool regionIsCorrect = false;
    qint64 regionLength = getCompleteSearchRegion(regionIsCorrect, msaEditor->getAlignmentLen()).length;

    SAFE_POINT(regionLength > 0 && true == regionIsCorrect,
               "Incorrect region length when enabling/disabling the pattern search button.", false);

    if (minMatch > regionLength) {
        return false;
    }
    return true;
}

void FindPatternMsaWidget::sl_onSelectedRegionChanged(){
    /*
    if(!currentSelection.isEmpty()){
        U2Region firstReg = currentSelection->getSelectedRegions().first();
        editStart->setText(QString::number(firstReg.startPos + 1));
        editEnd->setText(QString::number(firstReg.endPos()));

        if (currentSelection->getSelectedRegions().size() == 2) {
            U2Region secondReg = currentSelection->getSelectedRegions().last();
            SAFE_POINT(annotatedDnaView->getSequenceInFocus() != NULL, tr("Sequence in focus is NULL"), );
            int seqLen = annotatedDnaView->getSequenceInFocus()->getSequenceLength();
            bool circularSelection = (firstReg.startPos == 0 && secondReg.endPos() == seqLen)
                    || (firstReg.endPos() == seqLen && secondReg.startPos == 0);
            if (circularSelection) {
                if (secondReg.startPos == 0) {
                    editEnd->setText(QString::number(secondReg.endPos()));
                } else {
                    editStart->setText(QString::number(secondReg.startPos + 1));
                }
            }
        }
    }else{
        SAFE_POINT(annotatedDnaView->getSequenceInFocus() != NULL, "No sequence in focus, with active search tab in options panel",);
        editStart->setText(QString::number(1));
        editEnd->setText(QString::number(annotatedDnaView->getSequenceInFocus()->getSequenceLength()));
    }
    */
    regionIsCorrect = true;
    boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));
    checkState();
}

void FindPatternMsaWidget::updatePatternText(int previousAlgorithm) {
    // Save a previous state.
    if (FindAlgorithmPatternSettings_RegExp == previousAlgorithm) {
        patternRegExp = textPattern->toPlainText();
    }
    else {
        patternString = textPattern->toPlainText();
    }

    // Set a new state.
    if (FindAlgorithmPatternSettings_RegExp == selectedAlgorithm) {
        textPattern->setText(patternRegExp);
    }
    else {
        textPattern->setText(patternString);
    }
    setCorrectPatternsString();
}

void FindPatternMsaWidget::validateCheckBoxSize(QCheckBox* checkBox, int requiredWidth) {
    QFont font = checkBox->font();
    QFontMetrics checkBoxMetrics(font, checkBox);
    QString text = checkBox->text();

    if(text.contains('\n')) {
        return;
    }

    int lastSpacePos = 0;
    QString wrappedText = "";
    int startPos = 0;
    QRect textRect = checkBoxMetrics.boundingRect(text);
    if(textRect.width() <= requiredWidth) {
        return;
    }
    int length = text.length();
    for(int endPos = 0; endPos < length; endPos++) {
        if(' ' == text.at(endPos) || endPos == length - 1) {
            if(endPos-1 <= startPos) {
                wrappedText = "";
            }
            else {
                wrappedText = text.mid(startPos, endPos - startPos - 1);
            }
            textRect = checkBoxMetrics.boundingRect(wrappedText);
            if(textRect.width() > requiredWidth && 0 != lastSpacePos) {
                startPos = endPos;
                text[lastSpacePos] = '\n';
            }
            lastSpacePos = endPos;
        }
    }
    checkBox->setText(text);
}

void FindPatternMsaWidget::sl_toggleExtendedAlphabet() {
    verifyPatternAlphabet();
    sl_activateNewSearch(true);
}

void FindPatternMsaWidget::sl_activateNewSearch(bool forcedSearch){
    /*
    if(loadFromFileGroupBox->isChecked()) {
        stopCurrentSearchTask();
        if(filePathLineEdit->text().isEmpty()){
            return;
        }
        LoadPatternsFileTask *loadTask = new LoadPatternsFileTask(filePathLineEdit->text());
        connect(loadTask, SIGNAL(si_stateChanged()), SLOT(sl_loadPatternTaskStateChanged()));
        AppContext::getTaskScheduler()->registerTopLevelTask(loadTask);
    } else {
    */
    QList<NamePattern> newPatterns = updateNamePatterns();
    if(isSearchPatternsDifferent(newPatterns) || forcedSearch){
        patternList.clear();
        for(int i = 0; i < newPatterns.size();i++){
            newPatterns[i].first = QString::number(i);
            patternList.append(newPatterns[i].second);
        }
    } else {
        checkState();
        return;
    }

    stopCurrentSearchTask();
    initFindPatternTask(newPatterns);
}

QList<NamePattern> FindPatternMsaWidget::updateNamePatterns() {
    U2OpStatus2Log os;
    QList<NamePattern> newPatterns = getPatternsFromTextPatternField(os);

    nameList.clear();
    foreach(const NamePattern & np, newPatterns) {
        nameList.append(np.first);
    }
    return newPatterns;
}

void FindPatternMsaWidget::sl_prevButtonClicked() {
    if (!resultIterator.goPrevResult()) {
        resultIterator.goEnd();
    }
    showCurrentResult();
}

void FindPatternMsaWidget::sl_nextButtonClicked() {
    if (!resultIterator.goNextResult()) {
        resultIterator.goBegin();
    }
    showCurrentResult();
}

void FindPatternMsaWidget::showCurrentResult() const {
    resultLabel->setText(tr("Results: %1/%2").arg(QString::number(resultIterator.getGlobalPos())).arg(QString::number(resultIterator.getTotalCount())));
    CHECK(resultIterator.getTotalCount() >= resultIterator.getGlobalPos(), );
    MaEditorSequenceArea* seqArea = msaEditor->getUI()->getSequenceArea();

    const U2Region& currentRegion = resultIterator.currentResult();
    MaEditorSelection sel(QPoint(currentRegion.startPos, resultIterator.getRow()), currentRegion.length, 1);
    seqArea->setSelection(sel, true);
    seqArea->centerPos(sel.topLeft());
}

void FindPatternMsaWidget::sl_onEnterPressed(){
    if(nextPushButton->isEnabled()){
        nextPushButton->click();
    }
}

void FindPatternMsaWidget::sl_onShiftEnterPressed(){
    if(prevPushButton->isEnabled()){
        prevPushButton->click();
    }
}

bool FindPatternMsaWidget::isSearchPatternsDifferent(const QList<NamePattern> &newPatterns) const {
    if(patternList.size() != newPatterns.size()){
        return true;
    }
    foreach (const NamePattern &s, newPatterns) {
        if (!patternList.contains(s.second)) {
            return true;
        }
    }
    return false;
}

void FindPatternMsaWidget::stopCurrentSearchTask(){
    if(searchTask != NULL){
        if(!searchTask->isCanceled() && searchTask->getState() != Task::State_Finished){
            searchTask->cancel();
        }
        searchTask = NULL;
    }
    findPatternResults.clear();
    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);
    showCurrentResultAndStopProgress(0, 0);
}

void FindPatternMsaWidget::correctSearchInCombo(){
    if(boxRegion->itemData(boxRegion->currentIndex()).toInt() == RegionSelectionIndex_CurrentSelectedRegion){
        boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));
    }
}

void FindPatternMsaWidget::setUpTabOrder() const {
    QWidget::setTabOrder(nextPushButton, boxAlgorithm);
    QWidget::setTabOrder(boxRegion, editStart);
    QWidget::setTabOrder(editStart, editEnd);
    QWidget::setTabOrder(editEnd, removeOverlapsBox);
    QWidget::setTabOrder(removeOverlapsBox, boxMaxResult);
}

void FindPatternMsaWidget::startProgressAnimation() {
    resultLabel->setText(tr("Results:"));
    progressLabel->show();
    progressMovie->start();
}

} // namespace
