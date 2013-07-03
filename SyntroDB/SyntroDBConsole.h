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

#ifndef SYNTRODBCONSOLE_H
#define SYNTRODBCONSOLE_H

#include <QThread>
#include <QSettings>
#include "StoreStream.h"

class StoreClient;
class CFSClient;

class SyntroDBConsole : public QThread
{
	Q_OBJECT

public:
	SyntroDBConsole(QObject *parent);
	~SyntroDBConsole();

signals:
	void refreshStreamSource(int index);

protected:
	void run();

private:
	void showStatus();
	void showCounts();
	void showHelp();

	StoreClient *m_storeClient;
	CFSClient *m_CFSClient;
};

#endif // SYNTRODBCONSOLE_H
