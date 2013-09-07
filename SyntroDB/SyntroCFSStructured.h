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

#ifndef SYNTROCFSSTRUCTURED_H
#define SYNTROCFSSTRUCTURED_H

#include "SyntroCFS.h"

class SyntroCFSStructured : public SyntroCFS
{
public:
	SyntroCFSStructured(CFSClient *client, QString filePath);
	virtual ~SyntroCFSStructured();

	virtual bool cfsOpen(SYNTRO_CFSHEADER *cfsMsg);
	virtual void cfsRead(SYNTRO_EHEAD *ehead, SYNTRO_CFSHEADER *cfsMsg, SYNTROCFS_STATE *scs, unsigned int requestedIndex);
	virtual void cfsWrite(SYNTRO_EHEAD *ehead, SYNTRO_CFSHEADER *cfsMsg, SYNTROCFS_STATE *scs, unsigned int requestedIndex);

	virtual unsigned int cfsGetRecordCount();

private:
	QString m_indexPath;
};

#endif // SYNTROCFSSTRUCTURED_H
