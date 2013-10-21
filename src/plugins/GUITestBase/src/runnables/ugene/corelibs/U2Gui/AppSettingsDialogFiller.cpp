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
#include "AppSettingsDialogFiller.h"

#include "runnables/qt/ColorDialogFiller.h"
#include "api/GTWidget.h"
#include "api/GTTreeWidget.h"
#include "api/GTMouseDriver.h"
#include "api/GTComboBox.h"
#include "api/GTLineEdit.h"
#include "api/GTKeyboardDriver.h"

#include <QtGui/QApplication>
#include <QtGui/QTreeWidget>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QAbstractButton>

namespace U2{

#define GT_CLASS_NAME "AppSettingsDialogFiller"
#define GT_METHOD_NAME "run"
void AppSettingsDialogFiller::run(){

    QWidget* dialog = QApplication::activeModalWidget();
    GT_CHECK(dialog, "activeModalWidget is NULL");

    QTreeWidget* tree = qobject_cast<QTreeWidget*>(GTWidget::findWidget(os,"tree"));
    GT_CHECK(tree, "tree widger not found");

    QList<QTreeWidgetItem*> items = GTTreeWidget::getItems(tree->invisibleRootItem());
    foreach (QTreeWidgetItem* item,items){
        if(item->text(0)=="  Workflow Designer"){
            GTMouseDriver::moveTo(os,GTTreeWidget::getItemCenter(os,item));
            GTMouseDriver::click(os);
        }
    }
    if(itemStyle!=none){
        QComboBox *styleCombo = qobject_cast<QComboBox*>(GTWidget::findWidget(os,"styleCombo",dialog));
        GTComboBox::setCurrentIndex(os,styleCombo,itemStyle);
    }

    if(r!=-1){
        GTUtilsDialog::waitForDialog(os, new ColorDialogFiller(os,r,g,b));
        QWidget *colorWidget = GTWidget::findWidget(os,"colorWidget",dialog);
        GTWidget::click(os,colorWidget);
    }
    GTWidget::click(os,GTWidget::findWidget(os,"okButton"));
}
#undef GT_METHOD_NAME
#undef GT_CLASS_NAME

#define GT_CLASS_NAME "NewColorSchemeCreator"
#define GT_METHOD_NAME "run"
void NewColorSchemeCreator::run(){
    QWidget* dialog = QApplication::activeModalWidget();
    GT_CHECK(dialog, "activeModalWidget is NULL");

    QWidget* addSchemaButton = GTWidget::findWidget(os, "addSchemaButton");
    GT_CHECK (addSchemaButton, "addSchemaButton not found");

    GTUtilsDialog::waitForDialog(os, new CreateAlignmentColorSchemeDialogFiller(os, schemeName, al));
    GTWidget::click(os, addSchemaButton);
}
#undef GT_METHOD_NAME
#undef GT_CLASS_NAME

#define GT_CLASS_NAME "CreateAlignmentColorSchemeDialogFiller"
#define GT_METHOD_NAME "run"
void CreateAlignmentColorSchemeDialogFiller::run(){
    QWidget* dialog = QApplication::activeModalWidget();
    GT_CHECK(dialog, "activeModalWidget is NULL");

    QLineEdit* schemeNameLine = qobject_cast<QLineEdit*>(GTWidget::findWidget(os, "schemeName"));
    GT_CHECK(schemeNameLine, "schemeName lineEdit not found ");

    GTLineEdit::setText(os,schemeNameLine, schemeName);

    QComboBox *alphabetComboBox = qobject_cast<QComboBox*>(GTWidget::findWidget(os,"alphabetComboBox"));
    GT_CHECK(alphabetComboBox, "alphabetComboBox lineEdit not found ");

    GTComboBox::setCurrentIndex(os, alphabetComboBox, al);

    GTUtilsDialog::waitForDialog(os, new ColorSchemeDialogFiller(os));
    GTWidget::click(os, GTWidget::findWidget(os, "createButton"));
}
#undef GT_METHOD_NAME
#undef GT_CLASS_NAME

#define GT_CLASS_NAME "ColorSchemeDialogFiller"
#define GT_METHOD_NAME "run"
void ColorSchemeDialogFiller::run(){
    QWidget* dialog = QApplication::activeModalWidget();
    GT_CHECK(dialog, "activeModalWidget is NULL");

    QList<QAbstractButton* > list = dialog->findChildren<QAbstractButton*>();
    foreach(QAbstractButton* b, list){
        if (b->text().contains("ok",Qt::CaseInsensitive)){
            GTWidget::click(os, b);
            return;
        }
    }
    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["enter"]);//if ok button not found
}
#undef GT_METHOD_NAME
#undef GT_CLASS_NAME
//QDialogButtonBox *buttonBox;
}
