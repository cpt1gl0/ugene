/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
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

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <QtXml/QXmlInputSource>

#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>

#include <U2Core/AppContext.h>
#include <U2Core/LoadRemoteDocumentTask.h>
#include <U2Core/Log.h>
#include <U2Core/L10n.h>
#include <U2Core/GUrlUtils.h>
#include <U2Core/MultiTask.h>
#include <U2Core/Settings.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/DASSource.h>
#include <U2Core/U2SafePoints.h>

#include <U2Gui/LastUsedDirHelper.h>

#include "DownloadRemoteFileDialog.h"
#include "ui/ui_DownloadRemoteFileDialog.h"
#include "OpenViewTask.h"

static const QString SAVE_DIR("downloadremotefiledialog/savedir");
static const QString HINT_STYLE_SHEET = "color:green; font:bold";

namespace U2 {

QString DownloadRemoteFileDialog::defaultDB("");

DownloadRemoteFileDialog::DownloadRemoteFileDialog(QWidget *p):QDialog(p), isQueryDB(false) {
    ui = new Ui_DownloadRemoteFileDialog;
    ui->setupUi(this);

    ui->dasfeaturesWidget->setResizeMode(QListView::Adjust);
    ui->dasBox->hide();
    ui->formatBox->hide();
    ui->formatLabel->hide();
    adjustSize();

    RemoteDBRegistry& registry = RemoteDBRegistry::getRemoteDBRegistry();
    const QList<QString> dataBases = registry.getDBs(); 
    foreach(const QString& dbName, dataBases) {
        ui->databasesBox->addItem(dbName, dbName);
    }

    DASSourceRegistry * dasRegistry = AppContext::getDASSourceRegistry();
    if (dasRegistry){
        const QList<DASSource>& dasSources = dasRegistry->getReferenceSources();
        foreach(const DASSource& s, dasSources){
            ui->databasesBox->addItem(s.getName(), s.getId());
        }
    }

    if (!defaultDB.isEmpty()) {
        int index = ui->databasesBox->findData(defaultDB);
        if (index != -1){
            ui->databasesBox->setCurrentIndex(index);
        }
    }

    ui->hintLabel->setStyleSheet( HINT_STYLE_SHEET );

    connect(ui->databasesBox, SIGNAL(currentIndexChanged ( int)), SLOT( sl_onDbChanged(int)));
    connect(ui->saveFilenameToolButton, SIGNAL(clicked()), SLOT(sl_saveFilenameButtonClicked()));
    connect(ui->hintLabel, SIGNAL(linkActivated(const QString&)), SLOT(sl_linkActivated(const QString& )));

    sl_onDbChanged(ui->databasesBox->currentIndex());

    setSaveFilename();
}

DownloadRemoteFileDialog::DownloadRemoteFileDialog( const QString& id, const QString& dbId, QWidget *p /*= NULL*/ ) 
    :QDialog(p), isQueryDB(false)
{
    ui = new Ui_DownloadRemoteFileDialog;
    ui->setupUi(this);

    ui->dasfeaturesWidget->setResizeMode(QListView::Adjust);
    ui->dasBox->hide();
    ui->formatBox->addItem(GENBANK_FORMAT);
    ui->formatBox->addItem(FASTA_FORMAT);
    adjustSize();

    ui->databasesBox->clear();
    const QString dbName = 
        dbId == EntrezUtils::NCBI_DB_PROTEIN ?  RemoteDBRegistry::GENBANK_PROTEIN : RemoteDBRegistry::GENBANK_DNA;
    ui->databasesBox->addItem(dbName,dbName);

    ui->idLineEdit->setText(id);
    ui->idLineEdit->setReadOnly(true);

    delete ui->hintLabel;
    ui->hintLabel = NULL;
    setMinimumSize( 500, 0 );

    connect(ui->saveFilenameToolButton, SIGNAL(clicked()), SLOT(sl_saveFilenameButtonClicked()));
    setSaveFilename();
}

const QString DOWNLOAD_REMOTE_FILE_DOMAIN = "DownloadRemoteFileDialog";

void DownloadRemoteFileDialog::sl_saveFilenameButtonClicked() {
    LastUsedDirHelper lod(DOWNLOAD_REMOTE_FILE_DOMAIN);
    QString filename = QFileDialog::getExistingDirectory(this, tr("Select directory to save"), lod.dir);
    if(!filename.isEmpty()) {
        ui->saveFilenameLineEdit->setText(filename);
        lod.url = filename;
    }
}

static const QString DEFAULT_FILENAME = "file.format";
void DownloadRemoteFileDialog::setSaveFilename() {
    QString dir = AppContext::getSettings()->getValue(SAVE_DIR, "").value<QString>();
    if(dir.isEmpty()) {
        dir = LoadRemoteDocumentTask::getDefaultDownloadDirectory();
        assert(!dir.isEmpty());
    }
    ui->saveFilenameLineEdit->setText(QDir::toNativeSeparators(dir));
}

QString DownloadRemoteFileDialog::getResourceId() const
{
    return ui->idLineEdit->text().trimmed();
}

QString DownloadRemoteFileDialog::getDBId() const
{
    int curIdx = ui->databasesBox->currentIndex();
    if (curIdx == -1){
        return QString("");
    }
    return ui->databasesBox->itemData(curIdx).toString();
}

QString DownloadRemoteFileDialog::getFullpath() const {
    return ui->saveFilenameLineEdit->text();
}

void DownloadRemoteFileDialog::accept()
{
    defaultDB = getDBId();
    
    QString resourceId = getResourceId();
    if( resourceId.isEmpty() ) {
        QMessageBox::critical(this, L10N::errorTitle(), tr("Resource id is empty!"));
        ui->idLineEdit->setFocus();
        return;
    }
    QString fullPath = getFullpath();
    if( ui->saveFilenameLineEdit->text().isEmpty() ) {
        QMessageBox::critical(this, L10N::errorTitle(), tr("No directory selected for saving file!"));
        ui->saveFilenameLineEdit->setFocus();
        return;
    }

    U2OpStatus2Log os;
    fullPath = GUrlUtils::prepareDirLocation(fullPath, os);

    if (fullPath.isEmpty()) {
        QMessageBox::critical(this, L10N::errorTitle(), os.getError());
        ui->saveFilenameLineEdit->setFocus();
        return;
    }        
    
    QString dbId = getDBId();
    QStringList resIds = resourceId.split(QRegExp("[\\s,;]+"));
    QList<Task*> tasks;

    if (isDefaultDb(dbId)){
        QString fileFormat;
        if (ui->formatBox->count() > 0) {
            fileFormat = ui->formatBox->currentText();
        }
        foreach (const QString& resId, resIds) {
            tasks.append( new LoadRemoteDocumentAndOpenViewTask(resId, dbId, fullPath, fileFormat) );
        }

        AppContext::getTaskScheduler()->registerTopLevelTask( new MultiTask("DownloadRemoteDocuments", tasks) );
    }else{ //DAS ID
        DASSourceRegistry * dasRegistry = AppContext::getDASSourceRegistry();
        if (dasRegistry){
            //get features
            QList<DASSource> featureSources;
            for(int i = 0; i < ui->dasfeaturesWidget->count(); i++){
                QListWidgetItem* item = ui->dasfeaturesWidget->item(i);
                if (item->checkState() == Qt::Checked){
                    QString featureId = item->data(Qt::UserRole).toString();
                    DASSource fSource = dasRegistry->findById(featureId);
                    if (fSource.isValid()){
                        featureSources.append(fSource);
                    }
                }
            }
            //get sequence
            DASSource refSource = dasRegistry->findById(dbId);
            if (refSource.isValid()){
                foreach (const QString& resId, resIds) {
                    tasks.append( new LoadDASDocumentsAndOpenViewTask(resId, fullPath, refSource, featureSources));
                }
                AppContext::getTaskScheduler()->registerTopLevelTask( new MultiTask("LoadDASDocuments", tasks, false, TaskFlags(TaskFlag_FailOnSubtaskCancel | TaskFlag_NoRun)) );
            }
        }
    }

    QDialog::accept();
}

DownloadRemoteFileDialog::~DownloadRemoteFileDialog() {
    AppContext::getSettings()->setValue(SAVE_DIR, ui->saveFilenameLineEdit->text());
    delete ui;
}

bool DownloadRemoteFileDialog::isDefaultDb(const QString& dbId){
    RemoteDBRegistry& registry = RemoteDBRegistry::getRemoteDBRegistry();

    return registry.hasDbId(dbId);
}

void DownloadRemoteFileDialog::sl_onDbChanged( int /*idx*/ ){
    QString dbId = getDBId();
    QString hint;
    QString description;
    if (isDefaultDb(dbId)) {
        RemoteDBRegistry& registry = RemoteDBRegistry::getRemoteDBRegistry();
        hint = description = registry.getHint(dbId);
        ui->dasBox->hide();
        adjustSize();
    } else {
        DASSourceRegistry * dasRegistry = AppContext::getDASSourceRegistry();
        if (dasRegistry){
            const DASSource& dasSource = dasRegistry->findById(dbId);
            if (dasSource.isValid()){
                description = dasSource.getDescription();
                hint = dasSource.getHint();

                ui->dasfeaturesWidget->clear();
                const QList<DASSource>& featureSources = dasRegistry->getFeatureSourcesByType(dasSource.getReferenceType());
                foreach(const DASSource& s, featureSources){
                    QListWidgetItem* item = new QListWidgetItem(s.getName());
                    item->setData(Qt::UserRole, s.getId());
                    item->setToolTip(s.getHint());
                    item->setCheckState(Qt::Checked);
                    ui->dasfeaturesWidget->addItem(item);
                }
            }
        }
        ui->dasBox->show();
    }

    setupHintText( hint );
    ui->idLineEdit->setToolTip(description);
}

void DownloadRemoteFileDialog::sl_linkActivated( const QString& link ){
    if (!link.isEmpty()){
        ui->idLineEdit->setText(link);
    }
}

void DownloadRemoteFileDialog::setupHintText( const QString &text ) {
    SAFE_POINT( NULL != ui && NULL != ui->hintLabel, "Invalid dialog content!", );
    const QString hintStart( tr( "Hint: " ) );
    const QString hintSample = ( text.isEmpty( ) ? tr( "Use database unique identifier." ) : text )
        + "<br>";
    const QString hintEnd( tr( "You can download multiple items by separating IDs with space "
        "or semicolon." ) );
    ui->hintLabel->setText( hintStart + hintSample + hintEnd );
}

} //namespace 
