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

#include "SyntroServer.h"
#include "SyntroTunnel.h"
#include "SyntroControlLib.h"
#include "SyntroThread.h"

// SyntroServer

SyntroServer::SyntroServer(QSettings *settings)
{
	m_settings = settings;

	// process SyntroControl-specific settings

	m_settings->beginGroup(SYNTROCONTROL_PARAMS_GROUP);

	if (!settings->contains(SYNTROCONTROL_PARAMS_LISTEN_LOCAL_SOCKET))
		settings->setValue(SYNTROCONTROL_PARAMS_LISTEN_LOCAL_SOCKET, SYNTRO_PRIMARY_SOCKET_LOCAL);			
	if (!settings->contains(SYNTROCONTROL_PARAMS_LISTEN_STATICTUNNEL_SOCKET))
		settings->setValue(SYNTROCONTROL_PARAMS_LISTEN_STATICTUNNEL_SOCKET, SYNTRO_PRIMARY_SOCKET_STATICTUNNEL);		

	m_socketNumber = m_settings->value(SYNTROCONTROL_PARAMS_LISTEN_LOCAL_SOCKET).toInt();
	m_staticTunnelSocketNumber = m_settings->value(SYNTROCONTROL_PARAMS_LISTEN_STATICTUNNEL_SOCKET).toInt();

	m_settings->endGroup();

	// use some standard settings also

	m_heartbeatSendInterval = m_settings->value(SYNTRO_PARAMS_HBINTERVAL).toInt() * SYNTRO_CLOCKS_PER_SEC;
	m_heartbeatTimeoutCount = m_settings->value(SYNTRO_PARAMS_HBTIMEOUT).toInt();

	m_componentData.init(qPrintable(settings->value(SYNTRO_PARAMS_APPNAME).toString()),
				COMPTYPE_CONTROL,
				settings->value(SYNTRO_PARAMS_HBINTERVAL).toInt(),
				settings->value(SYNTRO_PARAMS_LOCALCONTROL_PRI).toInt());
	m_listSyntroLinkSock = NULL;
	m_listStaticTunnelSock = NULL;
	m_hello = NULL;
}

SyntroServer::~SyntroServer()
{
	qDebug() << "SyntroServer has been deleted";
}


Hello *SyntroServer::getHelloObject()
{
	return m_hello;
}

void SyntroServer::initThread()
{
	int		i;

	for (i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++)
		m_components[i].inUse = false;
	for (i = 0; i < SYNTRO_MAX_CONNECTIONIDS; i++)
		m_connectionIDMap[i] = -1;

	loadStaticTunnels();
	
	m_multicastManager.m_server = this;
	m_multicastManager.m_myUID = m_componentData.getMyUID();

	m_dirManager.m_server = this;

	m_multicastIn = m_multicastOut = m_E2EIn = m_E2EOut = 0;
	m_multicastInRate = m_multicastOutRate = m_E2EInRate = m_E2EOutRate = 0;
	m_counterStart = SyntroClock();

	m_myUID = m_componentData.getMyUID();
	m_componentName = m_settings->value(SYNTRO_PARAMS_APPNAME).toString();
	addLogService();

	m_timer = startTimer(SYNTROSERVER_INTERVAL);
	m_lastOpenSocketsTime = SyntroClock();
}

void SyntroServer::loadStaticTunnels()
{
	int	size = m_settings->beginReadArray(SYNTROCONTROL_PARAMS_STATIC_TUNNELS);
	m_settings->endArray();

	if (size == 0) {										// set a dummy entry for convenience
		m_settings->beginWriteArray(SYNTROCONTROL_PARAMS_STATIC_TUNNELS);

		m_settings->setArrayIndex(0);
		m_settings->setValue(SYNTROCONTROL_PARAMS_STATIC_NAME, "Tunnel");
		m_settings->setValue(SYNTROCONTROL_PARAMS_STATIC_DESTIP_PRIMARY, "0.0.0.0");
		m_settings->setValue(SYNTROCONTROL_PARAMS_STATIC_DESTIP_BACKUP, "");

		m_settings->endArray();
		return;
	}

	// Now create entries for valid static tunnel sources

	m_settings->beginReadArray(SYNTROCONTROL_PARAMS_STATIC_TUNNELS);
	for (int i = 0; i < size; i++) {
		m_settings->setArrayIndex(i);
		QString primary = m_settings->value(SYNTROCONTROL_PARAMS_STATIC_DESTIP_PRIMARY).toString();
		QString backup = m_settings->value(SYNTROCONTROL_PARAMS_STATIC_DESTIP_BACKUP).toString();
		if (primary.length() == 0)
			continue;
		if (primary == "0.0.0.0")
			continue;

		SS_COMPONENT *component = getFreeComponent();
		if (component == NULL)
			continue;

		component->inUse = true;
		component->tunnelStatic = true;
		component->tunnelSource = true;
		component->tunnelStaticPrimary = primary;
		component->tunnelStaticBackup = backup;
		component->tunnelStaticName = m_settings->value(SYNTROCONTROL_PARAMS_STATIC_NAME).toString();

		component->syntroTunnel = new SyntroTunnel(this, component, NULL, NULL);
		component->state = ConnWFHeartbeat;
		emit UpdateSyntroStatusBox(component);
	}
	m_settings->endArray();
}

void SyntroServer::addLogService()
{
	m_settings->beginGroup(SYNTRO_PARAMS_LOG_GROUP);
	m_netLogEnabled = m_settings->value(SYNTRO_PARAMS_NET_LOG).toBool();
	m_settings->endGroup();

	if (m_netLogEnabled) {
		m_logServiceName = m_settings->value(SYNTRO_PARAMS_APPNAME).toString() + 
			m_settings->value(SYNTRO_PARAMS_COMPTYPE).toString() + 
			SYNTRO_LOG_SERVICE_TAG;

		// Rebuild directory entry

		m_componentData.DESetup();
		m_componentData.DEAddValue(DETAG_MSERVICE, m_logServiceName);
		m_componentData.DEComplete();

		// now generate multicast map entry

		m_logMap = m_multicastManager.MMAllocateMMap(&m_myUID, &m_myUID, 
			qPrintable(m_componentName), qPrintable(m_logServiceName), SYNTROCONTROL_LOG_PORT);
	}
}


void SyntroServer::exitThread()
{
	m_run = false;
	killTimer(m_timer);
	exit();
	
	for (int i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++)
		syCleanup(m_components + i);

	m_dirManager.DMShutdown();
	m_multicastManager.MMShutdown();

	if (m_hello != NULL)
		m_hello->exitThread();

	if (m_listSyntroLinkSock != NULL)
		delete m_listSyntroLinkSock;
	if (m_listStaticTunnelSock != NULL)
		delete m_listStaticTunnelSock;
}

bool SyntroServer::openSockets()
{
	int ret;

	if ((m_listSyntroLinkSock != NULL) && (m_listStaticTunnelSock != NULL))
		return true;										// in normal operating mode

	if ((SyntroClock() - m_lastOpenSocketsTime) < SYNTROSERVER_SOCKET_RETRY)
		return false;										// not time to try again

	m_lastOpenSocketsTime = SyntroClock();

	if (m_listSyntroLinkSock == NULL) {
		m_listSyntroLinkSock = getNewSocket(false);
		if (m_listSyntroLinkSock != NULL) {
			ret = m_listSyntroLinkSock->sockListen();
			if (ret == 0) {
				delete m_listSyntroLinkSock;
				m_listSyntroLinkSock = NULL;
			} else {
				logInfo("Local listener active");
				m_hello = new Hello(&m_componentData);
				m_hello->m_parentThread = this;
				m_hello->m_socketFlags = 1;			
				m_hello->resumeThread();
			}
		}
	}

	if (m_listStaticTunnelSock == NULL) {
		m_listStaticTunnelSock = getNewSocket(true);
		if (m_listStaticTunnelSock != NULL) {
			ret = m_listStaticTunnelSock->sockListen();
			if (ret == 0) {
				delete m_listStaticTunnelSock;
				m_listStaticTunnelSock = NULL;
			} else {
				logInfo("Static tunnel listener active");
			}
		}
	}
	return (m_listSyntroLinkSock != NULL) && (m_listStaticTunnelSock != NULL);
}


void SyntroServer::setComponentSocket(SS_COMPONENT *syntroComponent, SyntroSocket *sock)
{
	syntroComponent->sock = sock;
	syntroComponent->connectionID = sock->sockGetConnectionID(); // record the connection ID

	if (m_connectionIDMap[syntroComponent->connectionID] != -1) {
		logWarn(QString("Connection ID %1 allocted to slot %2 but should be free")
			.arg(syntroComponent->connectionID)
			.arg(m_connectionIDMap[syntroComponent->connectionID]));
	}

	m_connectionIDMap[syntroComponent->connectionID] = syntroComponent->index;	// set the map entry
}

SS_COMPONENT *SyntroServer::getComponentFromConnectionID(int connectionID)
{
	int componentIndex;
	if ((connectionID < 0) || (connectionID >= SYNTRO_MAX_CONNECTIONIDS)) {
		logWarn(QString("Tried to map out of range connection ID %1 to component").arg(connectionID));
		return NULL;
	}
	componentIndex = m_connectionIDMap[connectionID];
	if ((componentIndex < 0) || (componentIndex >= SYNTRO_MAX_CONNECTEDCOMPONENTS)) {
		logWarn(QString("Out of range component index %1 in map for connection ID %2").arg(componentIndex).arg(connectionID));
		return NULL;
	}
	return m_components + componentIndex;
}

SyntroSocket *SyntroServer::getNewSocket(bool staticTunnel)
{
	SyntroSocket *sock;
	int	retVal;

	sock = new SyntroSocket(this);
	if (sock == NULL)
		return sock;
	if (!staticTunnel)
		retVal = sock->sockCreate(m_socketNumber, SOCK_SERVER, 1);	
	else
		retVal = sock->sockCreate(m_staticTunnelSocketNumber, SOCK_SERVER, 1);	// create with reuse flag set to avoid bind failing on restart
	if (retVal == 0) {
		delete sock;
		return NULL;
	}
	if (!staticTunnel)
		sock->sockSetAcceptMsg(SYNTROSERVER_ONACCEPT_MESSAGE);
	else
		sock->sockSetAcceptMsg(SYNTROSERVER_ONACCEPT_STATICTUNNEL_MESSAGE);
	return sock;
}

//	SyConnected - handle connected outgoing calls (tunnel sources)

bool	SyntroServer::syConnected(SS_COMPONENT *syntroComponent)
{
	if (!syntroComponent->tunnelSource) {
		logWarn("Connected message on component that is not a tunnel source");
		return false;
	}
	if (!syntroComponent->syntroTunnel->m_connectInProgress) {
		logWarn("Connected message on component that is not expecting a connected message");
		return false;
	}
	syntroComponent->syntroTunnel->connected();
	syntroComponent->lastHeartbeatReceived = SyntroClock();
	syntroComponent->lastHeartbeatSent = SyntroClock() - m_heartbeatSendInterval;
	syntroComponent->heartbeatInterval = m_heartbeatSendInterval;
	syntroComponent->state = ConnWFHeartbeat;
	if (syntroComponent->dirManagerConnComp == NULL)
		syntroComponent->dirManagerConnComp = m_dirManager.DMAllocateConnectedComponent(syntroComponent);
	return	true;
}


//	SyAccept - handle incoming calls

bool	SyntroServer::syAccept(bool staticTunnel)
{

	char IPStr[SYNTRO_MAX_NONTAG];
	SYNTRO_IPADDR componentIPAddr;
	int componentPort;
	SS_COMPONENT *component;
	SyntroSocket *sock;
	int bufSize = SYNTRO_MESSAGE_MAX * 3;
	bool retVal;

	sock = new SyntroSocket(this);
	if (!staticTunnel)
		retVal = m_listSyntroLinkSock->sockAccept(*sock, IPStr, &componentPort);
	else
		retVal = m_listStaticTunnelSock->sockAccept(*sock, IPStr, &componentPort);
	if (!retVal) {
		delete sock;
		return false;
	}
	sock->sockSetConnectMsg(SYNTROSERVER_ONCONNECT_MESSAGE);
	sock->sockSetCloseMsg(SYNTROSERVER_ONCLOSE_MESSAGE);
	sock->sockSetReceiveMsg(SYNTROSERVER_ONRECEIVE_MESSAGE);
	sock->sockSetSendMsg(SYNTROSERVER_ONSEND_MESSAGE);
	logDebug(QString("Accepted call from %1 port %2").arg(IPStr).arg(componentPort));
	convertIPStringToIPAddr(IPStr, componentIPAddr);

	component = findComponent(componentIPAddr, componentPort);	// see if know about this client already
	if (component != NULL) {								// do know about this one
		if (component->sock != NULL) {
			delete component->sock;
			delete component->syntroLink;
		}
		memcpy(component->compIPAddr, componentIPAddr, SYNTRO_IPADDR_LEN);
		component->compPort = componentPort;
		component->syntroLink = new SyntroLink();
	} else {
		component = getFreeComponent();
		if (component == NULL) {							// too many components!
			delete sock;
			return false;
		}
		memcpy(component->compIPAddr, componentIPAddr, SYNTRO_IPADDR_LEN);
		component->compPort = componentPort;
		component->syntroLink = new SyntroLink();
	}
	component->inUse = true;
	setComponentSocket(component, sock);					// configure component to use this socket
	sock->sockSetReceiveBufSize(bufSize);
	sock->sockSetSendBufSize(bufSize);

	component->lastHeartbeatReceived = SyntroClock();
	component->heartbeatInterval = m_heartbeatSendInterval;	// use this until we get it from received heartbeat
	component->state = ConnWFHeartbeat;
	return	true;
}

//	SyClose - received a close indication on a Syntro connection

void	SyntroServer::syClose(SS_COMPONENT *syntroComponent)
{
	TRACE1("Closing %s", qPrintable(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
	syCleanup(syntroComponent);
	emit UpdateSyntroStatusBox(syntroComponent);
	emit DMDisplay(&m_dirManager);
}

void	SyntroServer::syCleanup(SS_COMPONENT *syntroComponent)
{
	if (syntroComponent != NULL) {
		if (syntroComponent->inUse) {
			m_dirManager.DMDeleteConnectedComponent(syntroComponent->dirManagerConnComp);
			syntroComponent->dirManagerConnComp = NULL;
			if (syntroComponent->dirEntry != NULL) {
				free(syntroComponent->dirEntry);
				syntroComponent->dirEntry = NULL;
			}
			if (syntroComponent->tunnelSource) {
				syntroComponent->syntroTunnel->close();
			} else {
				syntroComponent->inUse = false;				// only if not tunnel source - tunnel source reuses component
			}
			if (syntroComponent->syntroLink != NULL) {
				delete syntroComponent->syntroLink;
				syntroComponent->syntroLink = NULL;
			}
			if (syntroComponent->sock != NULL) {
				delete syntroComponent->sock;
				if ((syntroComponent->connectionID >= 0) && (syntroComponent->connectionID < SYNTRO_MAX_CONNECTIONIDS))
					m_connectionIDMap[syntroComponent->connectionID] = -1;
				else
					logWarn(QString("Slot %1 has out of range connection ID %2")
						.arg(syntroComponent->index).arg(syntroComponent->connectionID));
			}
			syntroComponent->sock = NULL;
			syntroComponent->state = ConnIdle;
		}
	}
}


//	GetFreeComponent - get the next free SS_COMPONENT structure

SS_COMPONENT	*SyntroServer::getFreeComponent()
{
	int	i;
	SS_COMPONENT	*component;

	component = m_components;
	for (i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++, component++) {
		if (!component->inUse) {
			component->inUse = false;
			component->tunnelDest = false;
			component->tunnelSource = false;
			component->tunnelStatic = false;
			component->syntroLink = NULL;
			component->sock = NULL;
			component->syntroTunnel = NULL;
			component->dirEntry = NULL;
			component->dirEntryLength = 0;
			component->index = i;
			component->dirManagerConnComp = m_dirManager.DMAllocateConnectedComponent(component);
			return component;
		}
	}
	logError("Too many components in component table");
	return NULL;
}

//	SendSyntroMessage allows a message to be sent to a specific Component
//

bool SyntroServer::sendSyntroMessage(SYNTRO_UID *uid, int cmd, SYNTRO_MESSAGE *message, int length, int priority)
{
	SS_COMPONENT *syntroComponent = m_components;

	for (int i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++, syntroComponent++) {
		if (syntroComponent->inUse && (syntroComponent->state >= ConnWFHeartbeat)) {
			if (!compareUID(uid, &(syntroComponent->heartbeat.hello.componentUID)))
				continue;

			// send over link to component
			if (syntroComponent->syntroLink != NULL) {
				TRACE1("\nSend to %s", qPrintable(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
				syntroComponent->syntroLink->send(cmd, length, priority, (SYNTRO_MESSAGE *)message);
				syntroComponent->syntroLink->trySending(syntroComponent->sock);
				return true;
			}
		}
	}

	free(message);
	logWarn(QString("Failed sending message to %1").arg(qPrintable(displayUID(uid))));
	return false;
}

//	processReceivedData - handles data received from SyntroLinks
//


void	SyntroServer::processReceivedData(SS_COMPONENT *syntroComponent)
{
	int cmd;
	int length;
	SYNTRO_MESSAGE *message;
	int priority;

	QMutexLocker locker(&m_lock);

	if (syntroComponent->syntroLink == NULL) {
		logWarn("Received data on socket with no SCL");
		return;
	}
	syntroComponent->syntroLink->tryReceiving(syntroComponent->sock);

	for (priority = SYNTROLINK_HIGHPRI; priority <= SYNTROLINK_LOWPRI; priority++) {
		if (!syntroComponent->syntroLink->receive(priority, &cmd, &length, &message))
			continue;
		if (syntroComponent->state < ConnNormal) {
			if (syntroComponent->tunnelSource) {
				TRACE2("Received %d from %s", cmd, qPrintable(displayUID(&syntroComponent->syntroTunnel->m_helloEntry.hello.componentUID)));
			} else {
				TRACE3("Received %d from %s port %d", cmd, qPrintable(displayIPAddr(syntroComponent->compIPAddr)), syntroComponent->compPort);
			}
		} else {
			TRACE2("Received %d from %s", cmd, qPrintable(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
		}
		processReceivedDataDemux(syntroComponent, cmd, length, message);
	}
}

void SyntroServer::processReceivedDataDemux(SS_COMPONENT *syntroComponent, int cmd, int length, SYNTRO_MESSAGE *message)
{
	SYNTRO_HEARTBEAT *heartbeat;
	SYNTRO_SERVICE_LOOKUP *serviceLookup;

	switch (cmd) {
		case SYNTROMSG_HEARTBEAT:						// Syntro client heartbeat
			if (length < (int)sizeof(SYNTRO_HEARTBEAT)) {
				logWarn(QString("Heartbeat was too short %1").arg(length));
				free(message);
				break;
			}
			heartbeat = (SYNTRO_HEARTBEAT *)message;
			heartbeat->hello.componentName[SYNTRO_MAX_COMPNAME - 1] = 0;	// make sure strings are 0 terminated
			heartbeat->hello.componentType[SYNTRO_MAX_COMPTYPE - 1] = 0;	// make sure strings are 0 terminated
	
			syntroComponent->lastHeartbeatReceived = SyntroClock();
			if (!syntroComponent->tunnelSource)			// tunnel sources use their configured value
				syntroComponent->heartbeatInterval = convertUC2ToInt(heartbeat->hello.interval) * SYNTRO_CLOCKS_PER_SEC;	// record advertised heartbeat interval
			memcpy(&(syntroComponent->heartbeat), message, sizeof(SYNTRO_HEARTBEAT));
			if (syntroComponent->state == ConnWFHeartbeat) {	// first time for heartbeat
				syntroComponent->state = ConnNormal;
				logInfo(QString("New component %1").arg(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
				if (!syntroComponent->tunnelSource && 
							(strcmp(syntroComponent->heartbeat.hello.componentType, COMPTYPE_CONTROL) == 0))
					syntroComponent->tunnelDest = true;
			}
			emit UpdateSyntroStatusBox(syntroComponent);
			length -= sizeof(SYNTRO_HEARTBEAT);
			if (length > 0)								// there must be a DE attached						
				setComponentDE((char *)message + sizeof(SYNTRO_HEARTBEAT), length, syntroComponent);
			if (!syntroComponent->tunnelSource && !syntroComponent->tunnelDest)
				sendHeartbeat(syntroComponent);			// need to respond if a normal component
			if (syntroComponent->tunnelDest)
				sendTunnelHeartbeat(syntroComponent);	// send a tunnel heartbeat if it is a tunnel dest
			free(message);
			break;

		case SYNTROMSG_E2E:
			forwardE2EMessage(message, length);
			break;

		case SYNTROMSG_MULTICAST_MESSAGE:				// a multicast message 
			forwardMulticastMessage(syntroComponent, cmd, message, length);	// forward on to the interested remotes
			free(message);
			break;

		case SYNTROMSG_MULTICAST_ACK:					// pMsg is multicast header
			m_multicastManager.MMProcessMulticastAck((SYNTRO_EHEAD *)message, length);
			free(message);
			break;

		case SYNTROMSG_SERVICE_LOOKUP_REQUEST:			// a Component has requested a service lookup
			if (length != sizeof(SYNTRO_SERVICE_LOOKUP)) {
				logWarn(QString("Wrong size service lookup request %1").arg(length));
				free(message);
				break;
			}
			serviceLookup = (SYNTRO_SERVICE_LOOKUP *)message;
			TRACE2("Got service lookup for %s, type %d", serviceLookup->servicePath, serviceLookup->serviceType);
			m_dirManager.DMFindService(&(syntroComponent->heartbeat.hello.componentUID), serviceLookup);
			sendSyntroMessage(&(syntroComponent->heartbeat.hello.componentUID), 
						SYNTROMSG_SERVICE_LOOKUP_RESPONSE, message, length, SYNTROLINK_MEDHIGHPRI);	
			break;

		case SYNTROMSG_SERVICE_LOOKUP_RESPONSE:
			m_multicastManager.MMProcessLookupResponse((SYNTRO_SERVICE_LOOKUP *)message, length);
			free(message);
			break;

		case SYNTROMSG_DIRECTORY_REQUEST:
			free(message);								// nothing useful in the request itself
			m_dirManager.DMBuildDirectoryMessage(sizeof(SYNTRO_DIRECTORY_RESPONSE), (char **)&message, &length, false);
			sendSyntroMessage(&(syntroComponent->heartbeat.hello.componentUID), 
						SYNTROMSG_DIRECTORY_RESPONSE, message, length, SYNTROLINK_LOWPRI);	
			break;

		default:
			if (message != NULL) {
				logWarn(QString("Unrecognized message %1 from %2")
					.arg(displayUID(&syntroComponent->heartbeat.hello.componentUID))
					.arg(cmd));
				free(message);
			}
			break;
	}
}

void	SyntroServer::setComponentDE(char *dirEntry, int length, SS_COMPONENT *syntroComponent)
{
	dirEntry[length-1] = 0;									// make sure 0 terminated!
	if (syntroComponent->dirEntry != NULL) {				// already have a DE - see if it has changed
		if (length == syntroComponent->dirEntryLength) {	// are same length
			if (memcmp(dirEntry, syntroComponent->dirEntry, length) == 0)
				return;										// hasn't changed - do nothing
		}
		free(syntroComponent->dirEntry);					// has changed
	}
	syntroComponent->dirEntry = (char *)malloc(length);
	syntroComponent->dirEntryLength = length;
	memcpy(syntroComponent->dirEntry, dirEntry, length);	// remember new DE
	syntroComponent->dirManagerConnComp->connectedComponentUID = syntroComponent->heartbeat.hello.componentUID;
	m_dirManager.DMProcessDE(syntroComponent->dirManagerConnComp, dirEntry, length);
	TRACE1("Updated component %s", qPrintable(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
	emit DMDisplay(&m_dirManager);
}

void	SyntroServer::sendHeartbeat(SS_COMPONENT *syntroComponent)
{
	unsigned char *pMsg;
	
	if (!syntroComponent->inUse)
		return;
	if (syntroComponent->syntroLink == NULL)
		return;
	pMsg = (unsigned char *)malloc(sizeof(SYNTRO_HEARTBEAT));
	SYNTRO_HEARTBEAT hb = m_componentData.getMyHeartbeat();
	memcpy(pMsg, &hb, sizeof(SYNTRO_HEARTBEAT));
	syntroComponent->syntroLink->send(SYNTROMSG_HEARTBEAT, sizeof(SYNTRO_HEARTBEAT), SYNTROLINK_MEDHIGHPRI, (SYNTRO_MESSAGE *)pMsg);
	syntroComponent->syntroLink->trySending(syntroComponent->sock);
	TRACE2("Sent response HB to %s from slot %d", qPrintable(displayUID(&syntroComponent->heartbeat.hello.componentUID)), syntroComponent->index);
}


void SyntroServer::sendTunnelHeartbeat(SS_COMPONENT *syntroComponent)
{
	int messageLength;
	unsigned char *message;
    SYNTRO_HEARTBEAT heartbeat;

	heartbeat = m_componentData.getMyHeartbeat();

	if (!syntroComponent->inUse)
		return;

	if (syntroComponent->syntroLink == NULL)
		return;

	if (syntroComponent->tunnelSource) {
		if (syntroComponent->syntroTunnel->m_connected) {
			m_dirManager.DMBuildDirectoryMessage(sizeof(SYNTRO_HEARTBEAT), (char **)&message, &messageLength, true); // generate a customized DE
			memcpy(message, &heartbeat, sizeof(SYNTRO_HEARTBEAT));
			if (syntroComponent->syntroLink != NULL) {
				syntroComponent->syntroLink->send(SYNTROMSG_HEARTBEAT, messageLength, 
								SYNTROLINK_MEDHIGHPRI, (SYNTRO_MESSAGE *)message);
				syntroComponent->syntroLink->trySending(syntroComponent->sock);
			}
		}
	} else {
		if ((syntroComponent->state > ConnWFHeartbeat) && syntroComponent->tunnelDest) {
			m_dirManager.DMBuildDirectoryMessage(sizeof(SYNTRO_HEARTBEAT), (char **)&message, &messageLength, true); // generate a customized DE
			memcpy(message, &heartbeat, sizeof(SYNTRO_HEARTBEAT));
			if (syntroComponent->syntroLink != NULL) {
				syntroComponent->syntroLink->send(SYNTROMSG_HEARTBEAT, messageLength, 
								SYNTROLINK_MEDHIGHPRI, (SYNTRO_MESSAGE *)message);
				syntroComponent->syntroLink->trySending(syntroComponent->sock);
			}
		}
	}
}


// SyntroServer message handlers

bool SyntroServer::processMessage(SyntroThreadMsg* msg)
{
	SS_COMPONENT *syntroComponent;

	switch(msg->message) {
		case HELLO_STATUS_CHANGE_MESSAGE:
			switch (msg->intParam) {
				case HELLO_BEACON:
					processHelloBeacon((HELLO *)msg->ptrParam);
					break;

				case HELLO_UP:
					processHelloUp((HELLOENTRY *)msg->ptrParam);
					break;

				case HELLO_DOWN:
					processHelloDown((HELLOENTRY *)msg->ptrParam);
					break;

				default:
					TRACE1("Illegal Hello state %d", msg->intParam);
					break;
			}
			free((void *)msg->ptrParam);
			break;

		case SYNTROTHREAD_TIMER_MESSAGE:
			syntroBackground();
			break;

		case SYNTROSERVER_ONCONNECT_MESSAGE:
			syntroComponent = getComponentFromConnectionID(msg->intParam);
			if (syntroComponent == NULL) {
				logWarn(QString("OnConnect on unmatched socket %1").arg(msg->intParam));
				return true;
			}
			if (!syntroComponent->inUse) {
				logWarn(QString("OnConnect on not in use component %1").arg(syntroComponent->index));
				return true;
			}
			syConnected(syntroComponent);
			break;

		case SYNTROSERVER_ONACCEPT_MESSAGE:
			syAccept(false);
			return true;
	
		case SYNTROSERVER_ONACCEPT_STATICTUNNEL_MESSAGE:
			syAccept(true);
			return true;

		case SYNTROSERVER_ONCLOSE_MESSAGE:
			syntroComponent = getComponentFromConnectionID(msg->intParam);
			if (syntroComponent == NULL) {
				logWarn(QString("OnClose on unmatched socket %1").arg(msg->intParam));
				return true;
			}
			if (!syntroComponent->inUse) {
				logWarn(QString("OnClose on not in use component %1").arg(syntroComponent->index));
				return true;
			}
			syClose(syntroComponent);
			return true;

		case SYNTROSERVER_ONRECEIVE_MESSAGE:
			syntroComponent = getComponentFromConnectionID(msg->intParam);
			if (syntroComponent == NULL) {
				logWarn(QString("OnReceive on unmatched socket %1").arg(msg->intParam));
				return true;
			}
			if (!syntroComponent->inUse) {
				logWarn(QString("OnReceive on not in use component %1").arg(syntroComponent->index));
				return true;
			}
			processReceivedData(syntroComponent);
			return true;

		case SYNTROSERVER_ONSEND_MESSAGE:
			syntroComponent = getComponentFromConnectionID(msg->intParam);
			if (syntroComponent == NULL) {
				logWarn(QString("OnSend on unmatched socket %1").arg(msg->intParam));
				return true;
			}
			if (!syntroComponent->inUse) {
				logWarn(QString("OnSend on not in use component %1").arg(syntroComponent->index));
				return true;
			}
			if (syntroComponent->syntroLink != NULL)
				syntroComponent->syntroLink->trySending(syntroComponent->sock);
			return true;
	}
	return true;
}

void SyntroServer::processHelloBeacon(HELLO *hello)
{
	TRACE1("Sending beacon response to %s", qPrintable(displayUID(&hello->componentUID)));
	m_hello->sendHelloBeaconResponse(hello);
}

void	SyntroServer::processHelloUp(HELLOENTRY *helloEntry)
{
	SS_COMPONENT *component;
	HELLO *myHello;
	SYNTRO_HEARTBEAT heartbeat;

	heartbeat = m_componentData.getMyHeartbeat();

	myHello = &(heartbeat.hello);
	if (strcmp(helloEntry->hello.componentType, COMPTYPE_CONTROL) != 0) {	// not a SyntroControl so must be a beacon
		return;
	}

	if (compareUID(&(helloEntry->hello.componentUID), &(myHello->componentUID)))
		return;												// this is me - ignore!

	TRACE2("Got Hello UP from %s %s", helloEntry->hello.componentName, 
					qPrintable(displayIPAddr(helloEntry->hello.IPAddr)));

	if (m_fastUIDLookup.FULLookup(&(helloEntry->hello.componentUID))) {
		// component already in table - should mean that this is one in a different mode
		TRACE1("Received hello up from %s", qPrintable(displayUID(&helloEntry->hello.componentUID)));
		return;
	}

	if (UIDHigher(&(helloEntry->hello.componentUID), &(myHello->componentUID)))
		return;												// the new UID is lower than mine - let that device establish the call

	if ((component = getFreeComponent()) == NULL) {
		logError(QString("No component table space for %1").arg(displayUID(&helloEntry->hello.componentUID)));
		return;
	}

	component->syntroTunnel = new SyntroTunnel(this, component, m_hello, helloEntry);

	component->inUse = true;
	component->tunnelSource = true;

	component->heartbeat.hello = helloEntry->hello;
	component->state = ConnWFHeartbeat;
}

void	SyntroServer::processHelloDown(HELLOENTRY *helloEntry)
{
	SS_COMPONENT *component;
	HELLOENTRY foundHelloEntry;
	SYNTRO_HEARTBEAT heartbeat;

	heartbeat = m_componentData.getMyHeartbeat();

	if (strcmp(helloEntry->hello.componentType, COMPTYPE_CONTROL) != 0)
		return;												// not a SyntroControl

	if (compareUID(&(helloEntry->hello.componentUID), &(heartbeat.hello.componentUID)))
		return;												// this is me - ignore!

	TRACE2("Got Hello DOWN from %s %s", helloEntry->hello.componentName, 
							qPrintable(displayIPAddr(helloEntry->hello.IPAddr)));

	if (m_hello->findComponent(&foundHelloEntry, &(helloEntry->hello.componentUID))) {
		// component still in hello table so leave channel (must be a primary/backup pair)
		return;
	}

	if ((component = (SS_COMPONENT *)m_fastUIDLookup.FULLookup(&(helloEntry->hello.componentUID))) == NULL) {
		TRACE1("Failed to find channel for %s", qPrintable(displayUID(&helloEntry->hello.componentUID)));
		return;
	}

	TRACE2("Deleting entry for SyntroControl %s %s", helloEntry->hello.componentName, 
									qPrintable(displayIPAddr(helloEntry->hello.IPAddr)));

	m_fastUIDLookup.FULDelete(&(helloEntry->hello.componentUID)); 

	syCleanup(component);

	if (component->syntroTunnel != NULL) {
		delete component->syntroTunnel;
		component->syntroTunnel = NULL;
	}

	component->inUse = false;
	emit UpdateSyntroStatusBox(component);
}

void	SyntroServer::syntroBackground()
{
	int i;
	SS_COMPONENT *syntroComponent;
	qint64 now;

	if (!m_run)
		return;

	if (!openSockets())
		return;												// don't do anything until sockets open

	now = SyntroClock();

	if ((now - m_counterStart) >= SYNTRO_CLOCKS_PER_SEC) {	// time to update rates
		emit serverMulticastUpdate(m_multicastIn, m_multicastInRate, m_multicastOut, m_multicastOutRate);
		emit serverE2EUpdate(m_E2EIn, m_E2EInRate, m_E2EOut, m_E2EOutRate);
		m_counterStart += SYNTRO_CLOCKS_PER_SEC;
		m_multicastInRate = m_multicastOutRate = m_E2EInRate = m_E2EOutRate = 0;
	}
	syntroComponent = m_components;
	for (i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++, syntroComponent++)
	{
		if (syntroComponent->inUse)
		{
			if (syntroComponent->tunnelSource) {			
				syntroComponent->syntroTunnel->tunnelBackground();
				if (syntroComponent->syntroTunnel->m_connected) {
					if (syntroTimerExpired(now, syntroComponent->lastHeartbeatSent, syntroComponent->heartbeatInterval)) {
						sendTunnelHeartbeat(syntroComponent);
						syntroComponent->lastHeartbeatSent = now;
					}
					processReceivedData(syntroComponent);
					syntroComponent->syntroLink->trySending(syntroComponent->sock);
				}

				if (syntroComponent->syntroTunnel->m_connectInProgress || syntroComponent->syntroTunnel->m_connected) {
					if (syntroTimerExpired(now, syntroComponent->lastHeartbeatReceived,	m_heartbeatTimeoutCount * syntroComponent->heartbeatInterval)) {
						if (!syntroComponent->tunnelStatic)
							logWarn(QString("Timeout on tunnel source to %1").arg(syntroComponent->syntroTunnel->m_helloEntry.hello.componentName));
						else
							logWarn(QString("Timeout on tunnel source ") + syntroComponent->tunnelStaticName);
						syCleanup(syntroComponent);
						emit UpdateSyntroStatusBox(syntroComponent);
					}
				}
			} else {										// if not tunnel source
				if (syntroComponent->syntroLink != NULL) {
					processReceivedData(syntroComponent);
					syntroComponent->syntroLink->trySending(syntroComponent->sock);
				}

				if (syntroTimerExpired(SyntroClock(), syntroComponent->lastHeartbeatReceived, m_heartbeatTimeoutCount * syntroComponent->heartbeatInterval)) {
					logWarn(QString("Timeout on %1").arg(syntroComponent->index));
					syCleanup(syntroComponent);
					emit UpdateSyntroStatusBox(syntroComponent);
				}
			}
		}
	}
	logServiceBackground();
	m_multicastManager.MMBackground();
}

void SyntroServer::logServiceBackground()
{
	QString bulkMsg;

	if (!m_netLogEnabled)
		return;

	if (m_logMap->head == NULL)
		return;												// nobody is registered

	QQueue<LogMessage> *log = activeStreamQueue();
	if (!log)
		return;

	int count = log->count();
	if (count < 1)
		return;

	for (int i = 0; i < count; i++) {
		LogMessage m = log->dequeue();
		bulkMsg += m.m_level + SYNTRO_LOG_COMPONENT_SEP
			+ m.m_timeStamp + SYNTRO_LOG_COMPONENT_SEP
			+ displayUID(&m_myUID) + SYNTRO_LOG_COMPONENT_SEP
			+ m_logServiceName + SYNTRO_LOG_COMPONENT_SEP
			+ m.m_msg + "\n";
	}

	QByteArray data = bulkMsg.toLatin1();
	int length = sizeof(SYNTRO_RECORD_HEADER) + data.length();

	SYNTRO_EHEAD *multiCast = createEHEAD(&(m_myUID), 
					SYNTROCONTROL_LOG_PORT, 
					&(m_myUID), 
					SYNTROCONTROL_LOG_PORT, 
					0, 
					length);

	if (multiCast) {
		SYNTRO_RECORD_HEADER *recordHead = (SYNTRO_RECORD_HEADER *)(multiCast + 1);
		convertIntToUC2(SYNTRO_RECORD_TYPE_LOG, recordHead->type);
		convertIntToUC2(0, recordHead->subType);
		convertIntToUC2(0, recordHead->param);
		convertIntToUC2(sizeof(SYNTRO_RECORD_HEADER), recordHead->headerLength);
		setSyntroTimestamp(&recordHead->timestamp);
		memcpy((char *)(recordHead + 1), data.constData(), data.length());
		m_multicastManager.MMForwardMulticastMessage(SYNTROMSG_MULTICAST_MESSAGE, (SYNTRO_MESSAGE *)multiCast,
						sizeof(SYNTRO_EHEAD) + length);
		free(multiCast);
	}
}


void	SyntroServer::forwardE2EMessage(SYNTRO_MESSAGE *syntroMessage, int len)
{
	SYNTRO_EHEAD *ehead;
	SS_COMPONENT *component;

	ehead = (SYNTRO_EHEAD *)syntroMessage;								
	if (len < (int)sizeof(SYNTRO_EHEAD)) {
		logWarn(QString("Got too small E2E %1").arg(len));
		free(syntroMessage);
		return;
	}
	m_E2EIn++;
	m_E2EInRate++;

	if ((component = (SS_COMPONENT *)m_fastUIDLookup.FULLookup(&(ehead->destUID))) != NULL) {

		if (component->inUse && (component->state >= ConnWFHeartbeat)) {
			if (component->syntroLink != NULL) {
				TRACE1("Send to %s", qPrintable(displayUID(&component->heartbeat.hello.componentUID)));
				component->syntroLink->send(SYNTROMSG_E2E, len, syntroMessage->flags & SYNTROLINK_PRI, syntroMessage);
				component->syntroLink->trySending(component->sock);
				m_E2EOut++;
				m_E2EOutRate++;
			} else {
				free(syntroMessage);
			}
		}
		return;
	}

//	Not found!

	logError(QString("Failed to E2E dest for %1").arg(displayUID(&ehead->destUID)));
	free(syntroMessage);
}


void	SyntroServer::forwardMulticastMessage(SS_COMPONENT *syntroComponent, int cmd, SYNTRO_MESSAGE *message, int length)
{
	if (!syntroComponent->inUse) {
		logWarn(QString("ForwardMessage on not in use component %1").arg(displayUID(&syntroComponent->heartbeat.hello.componentUID)));
		return;												// not in use - hmmm. Should not happen!
	}
	if (length < (int)sizeof(SYNTRO_EHEAD)) {
		logWarn(QString("ForwardMessage is too short %1").arg(length));
		return;												// not in use - hmmm. Should not happen!
	}
	m_multicastManager.MMForwardMulticastMessage(cmd, message, length);
}


//	FindComponent - find the component corresponding to the address in the parameter

SS_COMPONENT	*SyntroServer::findComponent(SYNTRO_IPADDR compAddr, int compPort)
{
	int	i;

	for (i = 0; i < SYNTRO_MAX_CONNECTEDCOMPONENTS; i++) {
		if (m_components[i].inUse) {
			if ((memcmp(m_components[i].compIPAddr, compAddr, SYNTRO_IPADDR_LEN) == 0) &&
				(m_components[i].compPort == compPort))
				return m_components+i;
		}
	}
	return NULL;
}

