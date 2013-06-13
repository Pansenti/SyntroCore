//
//  Copyright (c) 2012 Pansenti, LLC.
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

#ifndef CONFIGURATIONDLG_H
#define CONFIGURATIONDLG_H

#include "SyntroValidators.h"
#include <qdialog.h>
#include <qlabel.h>
#include <qsettings.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qformlayout.h>
#include <qfiledialog.h>

class CFSLabel : public QLabel
{
public:
	CFSLabel(const QString& text);

protected:
	void mousePressEvent (QMouseEvent *ev);
};

class ConfigurationDlg : public QDialog
{
	Q_OBJECT

friend class CFSLabel;

public:
	ConfigurationDlg(QWidget *parent, QSettings *settings);

public slots:
	void cancelButtonClick();
	void okButtonClick();

private:
	void layoutWidgets();
	void loadCurrentValues();

	int m_index;
	QSettings *m_settings;

	QPushButton *m_okButton;
	QPushButton *m_cancelButton;

	CFSLabel *m_storePath;
};

#endif // CONFIGURATIONDLG_H
