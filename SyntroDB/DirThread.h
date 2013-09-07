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

#ifndef DIRTHREAD_H
#define DIRTHREAD_H

#include <qlist.h>
#include <qdir.h>

#include "SyntroLib.h"

class DirThread : public SyntroThread
{
	Q_OBJECT

public:
	DirThread(const QString& storePath);
	~DirThread();
	
	QString getDirectory();
	
protected:
	void initThread();
	void timerEvent(QTimerEvent *event);
	void finishThread();

private:
	void buildDirString();
	void processDir(QDir dir, QString& dirString, QString relativePath);

	QString m_storePath;
	QString m_directory;
	QMutex m_lock;

	int m_timer;
	bool m_dbOnly;
};

#endif // DIRTHREAD_H

