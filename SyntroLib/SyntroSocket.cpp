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

#include "SyntroSocket.h"

int nextConnectionID = 0;										// used to allocate unique IDs to socket connections
QMutex nextConnectionIDLock;									// to control access to the unique ID generator

// SyntroSocket

SyntroSocket::SyntroSocket()
{
	clearSocket();
}

SyntroSocket::SyntroSocket(SyntroThread *thread)
{
	clearSocket();
	m_ownerThread = thread;
	nextConnectionIDLock.lock();			
	m_connectionID = nextConnectionID++;					// allocate a new connection ID
	TRACE1("Allocated new socket with connection ID %d", m_connectionID);
	if (nextConnectionID == SYNTRO_MAX_CONNECTIONIDS)
		nextConnectionID = 0;								// wrap around
	nextConnectionIDLock.unlock();
}

void SyntroSocket::clearSocket()
{
	m_ownerThread = NULL;
	m_onConnectMsg = -1;
	m_onCloseMsg = -1;
	m_onReceiveMsg = -1;
	m_onAcceptMsg = -1;
	m_onSendMsg = -1;
	m_TCPSocket = NULL;
	m_UDPSocket = NULL;
	m_server = NULL;
	m_state = -1;
}

//	Set nFlags = true for reuseaddr

int	SyntroSocket::sockCreate(int nSocketPort, int nSocketType, int nFlags)
{
	bool	ret;

	m_sockType = nSocketType;
	m_sockPort = nSocketPort;
	switch (m_sockType) {
		case SOCK_DGRAM:
			m_UDPSocket = new QUdpSocket(this);
			connect(m_UDPSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
			connect(m_UDPSocket, SIGNAL(stateChanged ( QAbstractSocket::SocketState) ), this, SLOT(onState( QAbstractSocket::SocketState)));
			if (nFlags)
				ret = m_UDPSocket->bind(nSocketPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
			else
				ret = m_UDPSocket->bind(nSocketPort);
			return ret;

		case SOCK_STREAM:
			m_TCPSocket = new QTcpSocket(this);
			connect(m_TCPSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
			connect(m_TCPSocket, SIGNAL(stateChanged( QAbstractSocket::SocketState) ), this, SLOT(onState( QAbstractSocket::SocketState)));
			return 1;

		case SOCK_SERVER:
			m_server = new QTcpServer(this);
			return 1;

		default:
			logError(QString("Socket create on illegal type %1").arg(nSocketType));
			return 0;
	}
}

bool SyntroSocket::sockConnect(const char *addr, int port)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for create %1").arg(m_sockType));
		return false;
	}
	m_TCPSocket->connectToHost(addr, port);
	return true;
}

bool SyntroSocket::sockAccept(SyntroSocket& sock, char *IpAddr, int *port)
{
	sock.m_TCPSocket = m_server->nextPendingConnection();
	strcpy(IpAddr, (char *)sock.m_TCPSocket->peerAddress().toString().toLocal8Bit().constData());
	*port = (int)sock.m_TCPSocket->peerPort();
	sock.m_sockType = SOCK_STREAM;
	sock.m_ownerThread = m_ownerThread;
	sock.m_state = QAbstractSocket::ConnectedState;
	return true;
}

bool SyntroSocket::sockClose()
{
	switch (m_sockType) {
		case SOCK_DGRAM:
			disconnect(m_UDPSocket, 0, 0, 0);
			m_UDPSocket->close();
			delete m_UDPSocket;
			m_UDPSocket = NULL;
			break;

		case SOCK_STREAM:
			disconnect(m_TCPSocket, 0, 0, 0);
			m_TCPSocket->close();
			delete m_TCPSocket;
			m_TCPSocket = NULL;
			break;

		case SOCK_SERVER:
			disconnect(m_server, 0, 0, 0);
			m_server->close();
			delete m_server;
			m_server = NULL;
			break;
	}
	clearSocket();
	return true;
}

int	SyntroSocket::sockListen()
{
	if (m_sockType != SOCK_SERVER) {
		logError(QString("Incorrect socket type for listen %1").arg(m_sockType));
		return false;
	}
	return m_server->listen(QHostAddress::Any, m_sockPort);
}


int	SyntroSocket::sockReceive(void *lpBuf, int nBufLen)
{
	switch (m_sockType) {
		case SOCK_DGRAM:
			return m_UDPSocket->read((char *)lpBuf, nBufLen);

		case SOCK_STREAM:
			if (m_state != QAbstractSocket::ConnectedState)
				return 0;
			return m_TCPSocket->read((char *)lpBuf, nBufLen);

		default:
			logError(QString("Incorrect socket type for receive %1").arg(m_sockType));
			return -1;
	}
}

int	SyntroSocket::sockSend(void *lpBuf, int nBufLen)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for send %1").arg(m_sockType));
		return false;
	}
	if (m_state != QAbstractSocket::ConnectedState)
		return 0;
	return m_TCPSocket->write((char *)lpBuf, nBufLen); 
}

bool SyntroSocket::sockEnableBroadcast(int)
{
    return true;
}

bool SyntroSocket::sockSetReceiveBufSize(int nSize)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for SetReceiveBufferSize %1").arg(m_sockType));
		return false;
	}
	m_TCPSocket->setReadBufferSize(nSize);
	return true;
}

bool SyntroSocket::sockSetSendBufSize(int)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for SetSendBufferSize %1").arg(m_sockType));
		return false;
	}

//	logDebug(QString("SetSendBufferSize not implemented"));
	return true;
}


int		SyntroSocket::sockSendTo(const void *buf, int bufLen, int hostPort, char *host)
{
	if (m_sockType != SOCK_DGRAM) {
		logError(QString("Incorrect socket type for SendTo %1").arg(m_sockType));
		return false;
	}
	if (host == NULL)
		return m_UDPSocket->writeDatagram((const char *)buf, bufLen, getMyBroadcastAddress(), hostPort);	
	else
		return m_UDPSocket->writeDatagram((const char *)buf, bufLen, QHostAddress(host), hostPort);	
}

int	SyntroSocket::sockReceiveFrom(void *buf, int bufLen, char *IpAddr, unsigned int *port, int)
{
	QHostAddress ha;
	quint16 qport;

	if (m_sockType != SOCK_DGRAM) {
		logError(QString("Incorrect socket type for ReceiveFrom %1").arg(m_sockType));
		return false;
	}
	int	nRet = m_UDPSocket->readDatagram((char *)buf, bufLen, &ha, &qport);
	*port = qport;
	strcpy(IpAddr, qPrintable(ha.toString()));
	return nRet;
}

int		SyntroSocket::sockPendingDatagramSize()
{
	if (m_sockType != SOCK_DGRAM) {
		logError(QString("Incorrect socket type for PendingDatagramSize %1").arg(m_sockType));
		return false;
	}
	return m_UDPSocket->pendingDatagramSize();
}


SyntroSocket::~SyntroSocket()
{
	if (m_sockType != -1)
		sockClose();
}

// SyntroSocket member functions

int SyntroSocket::sockGetConnectionID()
{
	return m_connectionID;
}

void SyntroSocket::sockSetThread(SyntroThread *thread)
{
	m_ownerThread = thread;
}

void SyntroSocket::sockSetConnectMsg(int msg)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for SetConnectMsg %1").arg(m_sockType));
		return;
	}
	m_onConnectMsg = msg;
    connect(m_TCPSocket, SIGNAL(connected()), this, SLOT(onConnect()));
}


void SyntroSocket::sockSetAcceptMsg(int msg)
{
	if (m_sockType != SOCK_SERVER) {
		logError(QString("Incorrect socket type for SetAcceptMsg %1").arg(m_sockType));
		return;
	}
	m_onAcceptMsg = msg;
	connect(m_server, SIGNAL(newConnection()), this, SLOT(onAccept()));

}

void SyntroSocket::sockSetCloseMsg(int msg)
{
	if (m_sockType != SOCK_STREAM) {
		logError(QString("Incorrect socket type for SetCloseMsg %1").arg(m_sockType));
		return;
	}
	m_onCloseMsg = msg;
	connect(m_TCPSocket, SIGNAL(disconnected()), this, SLOT(onClose()));
}

void SyntroSocket::sockSetReceiveMsg(int msg)
{
	m_onReceiveMsg = msg;

	switch (m_sockType) {
		case SOCK_DGRAM:
			connect(m_UDPSocket, SIGNAL(readyRead()), this, SLOT(onReceive()));
			break;

		case SOCK_STREAM:
			connect(m_TCPSocket, SIGNAL(readyRead()), this, SLOT(onReceive()), Qt::DirectConnection);
			break;

		default:
			logError(QString("Incorrect socket type for SetReceiveMsg %1").arg(m_sockType));
			break;
	}
}

void SyntroSocket::sockSetSendMsg(int msg)
{
	m_onSendMsg = msg;
	switch (m_sockType)
	{
		case SOCK_DGRAM:
			connect(m_UDPSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(onSend(qint64)));
			break;

		case SOCK_STREAM:
			connect(m_TCPSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(onSend(qint64)), Qt::DirectConnection);
			break;

		default:
			logError(QString("Incorrect socket type for SetSendMsg %1").arg(m_sockType));
			break;
	}
}

void SyntroSocket::onConnect()
{
	if (m_onConnectMsg != -1)
		m_ownerThread->postThreadMessage(m_onConnectMsg, m_connectionID, NULL);
}

void SyntroSocket::onAccept()
{
	if (m_onAcceptMsg != -1)
		m_ownerThread->postThreadMessage(m_onAcceptMsg, m_connectionID, NULL);
}

void SyntroSocket::onClose()
{
	if (m_onCloseMsg != -1)
		m_ownerThread->postThreadMessage(m_onCloseMsg, m_connectionID, NULL);
}

void SyntroSocket::onReceive()
{
	if (m_onReceiveMsg != -1)
		m_ownerThread->postThreadMessage(m_onReceiveMsg, m_connectionID, NULL);
}

void	SyntroSocket::onSend(qint64)
{
	if (m_onSendMsg != -1)
		m_ownerThread->postThreadMessage(m_onSendMsg, m_connectionID, NULL);
}

void SyntroSocket::onError(QAbstractSocket::SocketError errnum)
{
	switch (m_sockType) {
		case SOCK_DGRAM:
			if (errnum != QAbstractSocket::AddressInUseError)
				logDebug(QString("UDP socket error %1").arg(m_UDPSocket->errorString()));

			break;

		case SOCK_STREAM:
			logDebug(QString("TCP socket error %1").arg(m_TCPSocket->errorString()));
			break;
	}
}

void	SyntroSocket::onState(QAbstractSocket::SocketState socketState)
{
	switch (m_sockType)
	{
		case SOCK_DGRAM:
			logDebug(QString("UDP socket state %1").arg(socketState));
			break;

		case SOCK_STREAM:
			logDebug(QString("TCP socket state %1").arg(socketState));
			break;
	}
	if ((socketState == QAbstractSocket::UnconnectedState) && (m_state < QAbstractSocket::ConnectedState))
		onClose();									// no signal generated in this situation
	m_state = socketState;
}
