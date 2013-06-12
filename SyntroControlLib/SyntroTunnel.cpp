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

#include "SyntroTunnel.h"
#include "SyntroControlLib.h"
#include "SyntroThread.h"

// SyntroTunnel


SyntroTunnel::SyntroTunnel(SyntroServer *server, SS_COMPONENT *syntroComponent, Hello *helloTask, HELLOENTRY *helloEntry)
{
	m_server = server;										// the server task
	m_comp = syntroComponent;								// the component to which this tunnel belongs
	if (helloEntry != NULL)
		m_helloEntry = *helloEntry;
	m_helloTask = helloTask;
	m_connected = false;
	m_connectInProgress = false;
	m_connWait = SyntroClock() - ENDPOINT_CONNWAIT;
}

SyntroTunnel::~SyntroTunnel()
{
}


bool SyntroTunnel::connect()
{
	int	returnValue;
	char str[1024];
	int bufSize = SYNTRO_MESSAGE_MAX * 3;
	qint64 now = SyntroClock();

	m_connectInProgress = false;
	m_connected = false;

	if (!syntroTimerExpired(now, m_connWait, ENDPOINT_CONNWAIT))
		return false;				// not time yet

	m_connWait = now;

	if (!m_comp->tunnelStatic) {

		//	try to find a SyntroControl in the appropriate state

		if (!m_helloTask->findComponent(&m_helloEntry, m_helloEntry.hello.componentName, (char *)COMPTYPE_CONTROL)) {// no SyntroControl 
			sprintf(str, "Waiting for SyntroControl %s in this run mode...", m_helloEntry.hello.componentName);
			TRACE0(str);
			return false;
		}
	}
	if (m_comp->sock != NULL){
		delete m_comp->sock;
		delete m_comp->syntroLink;
	}

	//	Create a new socket

	m_comp->sock = new SyntroSocket(m_server);
	m_server->setComponentSocket(m_comp, m_comp->sock);
	m_comp->syntroLink = new SyntroLink();
	returnValue = m_comp->sock->sockCreate(0, SOCK_STREAM);

	m_comp->sock->sockSetConnectMsg(SYNTROSERVER_ONCONNECT_MESSAGE);
	m_comp->sock->sockSetCloseMsg(SYNTROSERVER_ONCLOSE_MESSAGE);
	m_comp->sock->sockSetReceiveMsg(SYNTROSERVER_ONRECEIVE_MESSAGE);

	m_comp->sock->sockSetReceiveBufSize(bufSize);
	m_comp->sock->sockSetSendBufSize(bufSize);

	if (returnValue == 0)
		return false;

	if (!m_comp->tunnelStatic)
		returnValue = m_comp->sock->sockConnect(m_helloEntry.IPAddr, m_server->m_socketNumber);
	else
		returnValue = m_comp->sock->sockConnect(qPrintable(m_comp->tunnelStaticPrimary), m_server->m_staticTunnelSocketNumber);
	m_connectInProgress = true;
	m_comp->lastHeartbeatReceived = SyntroClock();
	return	true;
}

void	SyntroTunnel::connected()
{
	m_connected = true;
	m_connectInProgress = false;
	if (!m_comp->tunnelStatic) {
		TRACE1("Tunnel to %s connected", m_helloEntry.hello.componentName);
	} else {
		TRACE1("Tunnel %s connected", m_comp->tunnelStaticName);
	}
	m_comp->lastHeartbeatReceived = SyntroClock();
}

void SyntroTunnel::close()
{
	m_connected = false;
	m_connectInProgress = false;
}

void SyntroTunnel::tunnelBackground()
{	
	if ((!m_connectInProgress) && (!m_connected))
		connect();
}
