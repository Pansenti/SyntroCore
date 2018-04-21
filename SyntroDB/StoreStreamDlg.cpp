//
//  Copyright (c) 2013 Pansenti, LLC.
//	
//  This file is part of Syntro
//
//  Syntro is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Syntro is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Syntro.  If not, see <http://www.gnu.org/licenses/>.
//

#include <qvalidator.h>
#include <qmessagebox.h>

#include "StoreStreamDlg.h"
#include "StoreStream.h"
#include "SyntroDB.h"

StoreStreamDlg::StoreStreamDlg(QWidget *parent, int index)
	: QDialog(parent, Qt::WindowCloseButtonHint | Qt::WindowTitleHint)
{
	m_index = index;
	layoutWidgets();
	connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_okButton, SIGNAL(clicked()), this, SLOT(onOK()));
	loadCurrentValues();
	setWindowTitle("Configure Stream Storage");
}

void StoreStreamDlg::onOK()
{
	bool ok;

	int rotation_time = m_rotationTime->text().toInt(&ok);

	if (rotation_time < 1) {
		QMessageBox::warning(this, "Configure Stream Storage", "Rotation time must be at least 1");
		m_rotationTime->setText(QString::number(SYNTRODB_PARAMS_ROTATION_TIME_DEFAULT));
		m_rotationTime->setFocus();
		return;
	}

	int rotation_size = m_rotationSize->text().toInt();

	if (rotation_size < 1) {
		QMessageBox::warning(this, "Configure Stream Storage", "Rotation size must be at least 1");
		m_rotationSize->setText(QString::number(SYNTRODB_PARAMS_ROTATION_SIZE_DEFAULT));
		m_rotationSize->setFocus();
		return;
	}

	int deletion_time = m_deletionTime->text().toInt();

	if (deletion_time < 1) {
		QMessageBox::warning(this, "Configure Stream Storage", "Deletion time must be at least 1");
		m_deletionTime->setText(QString::number(SYNTRODB_PARAMS_DELETION_TIME_DEFAULT));
		m_deletionTime->setFocus();
		return;
	}

	int deletion_count = m_deletionCount->text().toInt();

	if (deletion_count < 2) {
		QMessageBox::warning(this, "Configure Stream Storage", "Deletion count must be at least 2");
		m_deletionCount->setText("2");
		m_deletionCount->setFocus();
		return;
	}

	QSettings *settings = SyntroUtils::getSettings();

	settings->beginWriteArray(SYNTRODB_PARAMS_STREAM_SOURCES);
	settings->setArrayIndex(m_index);

	if (m_formatCombo->currentIndex() == 1)
		settings->setValue(SYNTRODB_PARAMS_FORMAT, SYNTRO_RECORD_STORE_FORMAT_RAW);
	else
		settings->setValue(SYNTRODB_PARAMS_FORMAT, SYNTRO_RECORD_STORE_FORMAT_SRF);

	settings->setValue(SYNTRODB_PARAMS_CREATE_SUBFOLDER, (m_subFolderCheck->checkState() == Qt::Checked));
	settings->setValue(SYNTRODB_PARAMS_STREAM_SOURCE, m_streamName->text());

	//	Rotation policy

	switch (m_rotationPolicy->currentIndex()) {
		case 0:
			settings->setValue(SYNTRODB_PARAMS_ROTATION_POLICY, SYNTRODB_PARAMS_ROTATION_POLICY_TIME);
			break;

		case 1:
			settings->setValue(SYNTRODB_PARAMS_ROTATION_POLICY, SYNTRODB_PARAMS_ROTATION_POLICY_SIZE);
			break;

		default:
			settings->setValue(SYNTRODB_PARAMS_ROTATION_POLICY, SYNTRODB_PARAMS_ROTATION_POLICY_ANY);
			break;
	}

	switch (m_rotationTimeUnits->currentIndex()) {
	case 1:
		settings->setValue(SYNTRODB_PARAMS_ROTATION_TIME_UNITS, SYNTRODB_PARAMS_ROTATION_TIME_UNITS_HOURS);
		break;

	case 2:
		settings->setValue(SYNTRODB_PARAMS_ROTATION_TIME_UNITS, SYNTRODB_PARAMS_ROTATION_TIME_UNITS_DAYS);
		break;

	default:
		settings->setValue(SYNTRODB_PARAMS_ROTATION_TIME_UNITS, SYNTRODB_PARAMS_ROTATION_TIME_UNITS_MINUTES);
		break;
	}
	
	settings->setValue(SYNTRODB_PARAMS_ROTATION_TIME, rotation_time);
	settings->setValue(SYNTRODB_PARAMS_ROTATION_SIZE, rotation_size);

	//	Deletion policy

	switch (m_deletionPolicy->currentIndex()) {
		case 0:
			settings->setValue(SYNTRODB_PARAMS_DELETION_POLICY, SYNTRODB_PARAMS_DELETION_POLICY_TIME);
			break;

		case 1:
			settings->setValue(SYNTRODB_PARAMS_DELETION_POLICY, SYNTRODB_PARAMS_DELETION_POLICY_COUNT);
			break;

		default:
			settings->setValue(SYNTRODB_PARAMS_DELETION_POLICY, SYNTRODB_PARAMS_DELETION_POLICY_ANY);
			break;
	}

	if (m_deletionTimeUnits->currentIndex() == 1)
		settings->setValue(SYNTRODB_PARAMS_DELETION_TIME_UNITS, SYNTRODB_PARAMS_DELETION_TIME_UNITS_DAYS);
	else	
		settings->setValue(SYNTRODB_PARAMS_DELETION_TIME_UNITS, SYNTRODB_PARAMS_DELETION_TIME_UNITS_HOURS);

	settings->setValue(SYNTRODB_PARAMS_DELETION_TIME, deletion_time);
	settings->setValue(SYNTRODB_PARAMS_DELETION_COUNT, deletion_count);

	settings->endArray();
	delete settings;

	accept();
}

void StoreStreamDlg::loadCurrentValues()
{
	QSettings *settings = SyntroUtils::getSettings();

	settings->beginReadArray(SYNTRODB_PARAMS_STREAM_SOURCES);
	settings->setArrayIndex(m_index);

	m_streamName->setText(settings->value(SYNTRODB_PARAMS_STREAM_SOURCE).toString());

	if (settings->value(SYNTRODB_PARAMS_FORMAT, SYNTRO_RECORD_STORE_FORMAT_RAW).toString() == SYNTRO_RECORD_STORE_FORMAT_RAW)
		m_formatCombo->setCurrentIndex(1);
	else // if (settings->value(SYNTRODB_PARAMS_FORMAT) == SYNTRO_RECORD_STORE_FORMAT_SRF)
		m_formatCombo->setCurrentIndex(0);

	if (settings->value(SYNTRODB_PARAMS_CREATE_SUBFOLDER, true).toBool())
		m_subFolderCheck->setCheckState(Qt::Checked);
	else
		m_subFolderCheck->setCheckState(Qt::Unchecked);

	//	Rotation policy

	QString rotate = settings->value(SYNTRODB_PARAMS_ROTATION_POLICY, SYNTRODB_PARAMS_ROTATION_POLICY_DEFAULT).toString().toLower();
	
	if (rotate == SYNTRODB_PARAMS_ROTATION_POLICY_TIME)
		m_rotationPolicy->setCurrentIndex(0);
	else if (rotate == SYNTRODB_PARAMS_ROTATION_POLICY_SIZE)
		m_rotationPolicy->setCurrentIndex(1);
	else // SYNTRODB_PARAMS_ROTATION_POLICY_ANY
		m_rotationPolicy->setCurrentIndex(2);


	QString units = settings->value(SYNTRODB_PARAMS_ROTATION_TIME_UNITS, SYNTRODB_PARAMS_ROTATION_TIME_UNITS_DEFAULT).toString().toLower();

	if (units == SYNTRODB_PARAMS_ROTATION_TIME_UNITS_MINUTES)
		m_rotationTimeUnits->setCurrentIndex(0);
	else if (units == SYNTRODB_PARAMS_ROTATION_TIME_UNITS_HOURS)
		m_rotationTimeUnits->setCurrentIndex(1);
	else // SYNTRODB_PARAMS_ROTATION_TIME_UNITS_DAYS
		m_rotationTimeUnits->setCurrentIndex(2);

	int time = settings->value(SYNTRODB_PARAMS_ROTATION_TIME, SYNTRODB_PARAMS_ROTATION_TIME_DEFAULT).toInt();
	
	if (time < 1)
		time = 1;

	int size = settings->value(SYNTRODB_PARAMS_ROTATION_SIZE, SYNTRODB_PARAMS_ROTATION_SIZE_DEFAULT).toInt();

	if (size > SYNTRODB_PARAMS_ROTATION_SIZE_MAX)
		size = SYNTRODB_PARAMS_ROTATION_SIZE_MAX;
	else if (size < 1)
		size = 1;

	m_rotationTime->setText(QString::number(time));
	m_rotationSize->setText(QString::number(size));

	//	Deletion policy

	QString deletion = settings->value(SYNTRODB_PARAMS_DELETION_POLICY, SYNTRODB_PARAMS_DELETION_POLICY_DEFAULT).toString().toLower();

	if (deletion == SYNTRODB_PARAMS_DELETION_POLICY_TIME)
		m_deletionPolicy->setCurrentIndex(0);
	else if (deletion == SYNTRODB_PARAMS_DELETION_POLICY_COUNT)
		m_deletionPolicy->setCurrentIndex(1);
	else // SYNTRODB_PARAMS_DELETION_POLICY_ANY
		m_deletionPolicy->setCurrentIndex(2);

	units = settings->value(SYNTRODB_PARAMS_DELETION_TIME_UNITS, SYNTRODB_PARAMS_DELETION_TIME_UNITS_DEFAULT).toString().toLower();

	if (units == SYNTRODB_PARAMS_DELETION_TIME_UNITS_HOURS)
		m_deletionTimeUnits->setCurrentIndex(0);
	else // SYNTRODB_PARAMS_DELETION_TIME_UNITS_DAYS
		m_deletionTimeUnits->setCurrentIndex(1);

	time = settings->value(SYNTRODB_PARAMS_DELETION_TIME).toInt();

	if (time < 1)
		time = 1;

	int count = settings->value(SYNTRODB_PARAMS_DELETION_COUNT).toInt();

	if (count < 2)
		count = 2;

	m_deletionTime->setText(QString::number(time));
	m_deletionCount->setText(QString::number(count));

	settings->endArray();

	delete settings;
}

void StoreStreamDlg::layoutWidgets()
{
	QFormLayout *formLayout = new QFormLayout;

	QHBoxLayout *e = new QHBoxLayout;
	e->addStretch();

	m_formatCombo = new QComboBox;
	m_formatCombo->addItem("Structured file [srf])");
	m_formatCombo->addItem("Raw [raw]");
	m_formatCombo->setEditable(false);
	QHBoxLayout *a = new QHBoxLayout;
	a->addWidget(m_formatCombo);
	a->addStretch();
	formLayout->addRow(new QLabel("Format"), a);

	m_streamName = new QLineEdit;
	m_streamName->setMinimumWidth(200);
	m_streamName->setValidator(&m_validator);
	formLayout->addRow(new QLabel("Stream path"), m_streamName);

	m_subFolderCheck = new QCheckBox;
	formLayout->addRow(new QLabel("Create subfolder"), m_subFolderCheck);

	//	Rotation policy

	m_rotationPolicy = new QComboBox;
	m_rotationPolicy->addItem(SYNTRODB_PARAMS_ROTATION_POLICY_TIME);
	m_rotationPolicy->addItem(SYNTRODB_PARAMS_ROTATION_POLICY_SIZE);
	m_rotationPolicy->addItem(SYNTRODB_PARAMS_ROTATION_POLICY_ANY);
	m_rotationPolicy->setEditable(false);

	QHBoxLayout *b = new QHBoxLayout;
	b->addWidget(m_rotationPolicy);
	b->addStretch();
	formLayout->addRow(new QLabel("Rotation policy"), b);

	m_rotationTimeUnits = new QComboBox;
	m_rotationTimeUnits->addItem(SYNTRODB_PARAMS_ROTATION_TIME_UNITS_MINUTES);
	m_rotationTimeUnits->addItem(SYNTRODB_PARAMS_ROTATION_TIME_UNITS_HOURS);
	m_rotationTimeUnits->addItem(SYNTRODB_PARAMS_ROTATION_TIME_UNITS_DAYS);
	m_rotationTimeUnits->setEditable(false);
	QHBoxLayout *c = new QHBoxLayout;
	c->addWidget(m_rotationTimeUnits);
	c->addStretch();
	formLayout->addRow(new QLabel("Rotation time units"), c);
		
	m_rotationTime = new QLineEdit;
	m_rotationTime->setMaximumWidth(100);
	m_rotationTime->setValidator(new QIntValidator(1, 1000, this));
	formLayout->addRow(new QLabel("Rotation time"), m_rotationTime);

	m_rotationSize = new QLineEdit;
	m_rotationSize->setMaximumWidth(100);
	m_rotationSize->setValidator(new QIntValidator(1, SYNTRODB_PARAMS_ROTATION_SIZE_MAX, this));
	formLayout->addRow(new QLabel("Rotation size (MB)"), m_rotationSize);

	//	Deletion policy

	m_deletionPolicy = new QComboBox;
	m_deletionPolicy->addItem(SYNTRODB_PARAMS_DELETION_POLICY_TIME);
	m_deletionPolicy->addItem(SYNTRODB_PARAMS_DELETION_POLICY_COUNT);
	m_deletionPolicy->addItem(SYNTRODB_PARAMS_DELETION_POLICY_ANY);
	m_deletionPolicy->setEditable(false);
	QHBoxLayout *d = new QHBoxLayout;
	d->addWidget(m_deletionPolicy);
	d->addStretch();
	formLayout->addRow(new QLabel("Deletion policy"), d);

	m_deletionTimeUnits = new QComboBox;
	m_deletionTimeUnits->addItem(SYNTRODB_PARAMS_DELETION_TIME_UNITS_HOURS);
	m_deletionTimeUnits->addItem(SYNTRODB_PARAMS_DELETION_TIME_UNITS_DAYS);
	m_deletionTimeUnits->setEditable(false);
	QHBoxLayout *f = new QHBoxLayout;
	f->addWidget(m_deletionTimeUnits);
	f->addStretch();
	formLayout->addRow(new QLabel("Max age time units"), f);
		
	m_deletionTime = new QLineEdit;
	m_deletionTime->setMaximumWidth(100);
	m_deletionTime->setValidator(new QIntValidator(1, 1000, this));
	formLayout->addRow(new QLabel("Max age of file"), m_deletionTime);

	m_deletionCount = new QLineEdit;
	m_deletionCount->setMaximumWidth(100);
	m_deletionCount->setValidator(new QIntValidator(2, 10000, this));
	formLayout->addRow(new QLabel("Max files to keep"), m_deletionCount);

	QHBoxLayout *buttonLayout = new QHBoxLayout;

	m_okButton = new QPushButton("Ok");
	m_cancelButton = new QPushButton("Cancel");

	buttonLayout->addStretch();
	buttonLayout->addWidget(m_okButton);
	buttonLayout->addWidget(m_cancelButton);
	buttonLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(formLayout);
	mainLayout->addSpacing(20);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);
}

//----------------------------------------------------------
//
//	StoreLabel functions

StoreLabel::StoreLabel(const QString& text) : QLabel(text)
{
	setFrameStyle(QFrame::Sunken | QFrame::Panel);
}

void StoreLabel::mousePressEvent (QMouseEvent *)
{
	QFileDialog *fileDialog;

	fileDialog = new QFileDialog(this, "Store folder path");
	fileDialog->setFileMode(QFileDialog::DirectoryOnly);
	fileDialog->selectFile(text());

	if (fileDialog->exec())
		setText(fileDialog->selectedFiles().at(0));
}
