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

#ifndef SYNTROALERT_H
#define SYNTROALERT_H

#include <QMainWindow>
#include <qstringlist.h>

#include "ui_syntroalert.h"

#include "AlertClient.h"
#include "SyntroLib.h"
#include "SyntroServer.h"

class SyntroAlert : public QMainWindow
{
	Q_OBJECT

public:
	SyntroAlert(QWidget *parent = 0);
	
public slots:
	void newLogMsg(QByteArray bulkMsg);
	void activeClientUpdate(int count);
	void onClear();
	void onSave();
	void onBasicSetup();
	void onAbout();
    void onTypes();
    void onFields();

protected:
	void closeEvent(QCloseEvent *event);
	void timerEvent(QTimerEvent *event);
	
private:
	void saveWindowState();
	void restoreWindowState();
	void initStatusBar();
	void initGrid();
	void startControlServer();
	void parseMsgQueue();
    void addMessage(QString packedMsg);
    void updateTable(QStringList msg);
    int findRowInsertPosition(QStringList msg);
	bool filtered(QString type);
    void onTypesChange();
    void onFieldsChange();
    void validateViewFields();
    void setDefaultViewFields();

	Ui::SyntroAlertClass ui;

	SyntroServer *m_controlServer;
	AlertClient *m_client;
	int m_timer;
	QQueue<QByteArray> m_logQ;
	QMutex m_logMutex;

	int m_activeClientCount;
	QMutex m_activeClientMutex;

	QLabel *m_controlStatus;
	QLabel *m_activeClientStatus;

    QList<QStringList> m_entries;
    QList<int> m_viewFields;
    QStringList m_fieldLabels;
    QList<int> m_colWidths;
	QStringList m_currentTypes;
    int m_timestampCol;

	QString m_savePath;
};

#endif // SYNTROALERT_H
