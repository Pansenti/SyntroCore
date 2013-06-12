//
//  Copyright (c) 2012 Pansenti, LLC.
//	
//  This file is part of SyntroLib
//
//  SyntroLib is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  SyntroLib is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with SyntroLib.  If not, see <http://www.gnu.org/licenses/>.
//

#include "SyntroValidators.h"
#include "SyntroUtils.h"


QValidator::State ServiceNameValidator::validate(QString &input, int &pos) const
{
	if (pos == 0)
		return QValidator::Acceptable;						// empty string ok

	if (isReservedNameCharacter(input.at(pos-1).toLatin1())) 
		return QValidator::Invalid;
	if (pos >= SYNTRO_MAX_NAME)
		return QValidator::Invalid;

	return QValidator::Acceptable;
}

QValidator::State ServicePathValidator::validate(QString &input, int &pos) const
{
	if (pos == 0)
		return QValidator::Acceptable;						// empty string ok

	if (isReservedPathCharacter(input.at(pos-1).toLatin1())) 
		return QValidator::Invalid;
	if (pos >= SYNTRO_MAX_SERVPATH)
		return QValidator::Invalid;

	return QValidator::Acceptable;
}
