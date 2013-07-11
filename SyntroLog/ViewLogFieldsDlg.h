//
//  Copyright (c) 2012, 2013 Pansenti, LLC.
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

#ifndef VIEWLOGFIELDSDLG_H
#define VIEWLOGFIELDSDLG_H

#include <qobject.h>
#include <qdialog.h>
#include <qcheckbox.h>
#include <qlist.h>

#define LOGLEVEL_FIELD  0
#define TIMESTAMP_FIELD 1
#define UID_FIELD       2
#define APP_TYPE_FIELD  3
#define APP_NAME_FIELD  4
#define MESSAGE_FIELD   5
#define NUM_LOG_FIELDS  6


class ViewLogFieldsDlg : public QDialog
{
	Q_OBJECT

public:
    ViewLogFieldsDlg(QWidget *parent, QList<int> fields);

    QList<int> viewFields();

private:
    void layoutWindow(QList<int> fields);

    QCheckBox *m_check[NUM_LOG_FIELDS];
};

#endif // VIEWLOGFIELDSDLG_H
