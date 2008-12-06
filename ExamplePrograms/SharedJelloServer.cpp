/***********************************************************************
SharedJelloServer - Dedicated server program to allow multiple clients
to collaboratively smack around a Jell-O crystal.
Copyright (c) 2007 Oliver Kreylos

This file is part of the Virtual Jell-O interactive VR demonstration.

Virtual Jell-O is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

Virtual Jell-O is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with Virtual Jell-O; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <Misc/Timer.h>
#include <Misc/ThrowStdErr.h>

#include "SharedJelloServer.h"

/**********************************
Methods of class SharedJelloServer:
**********************************/

void* SharedJelloServer::listenThreadMethod(void)
	{
	/* Enable immediate cancellation of this thread: */
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	/* Process incoming connections until shut down: */
	while(true)
		{
		/* Wait for the next incoming connection: */
		#ifdef VERBOSE
		std::cout<<"SharedJelloServer: Waiting for client connection"<<std::endl<<std::flush;
		#endif
		Comm::TCPSocket clientSocket=listenSocket.accept();
		
		/* Connect the new client: */
		#ifdef VERBOSE
		std::cout<<"SharedJelloServer: Connecting new client from host "<<clientSocket.getPeerHostname(false)<<", port "<<clientSocket.getPeerPortId()<<std::endl<<std::flush;
		#endif
		
		/**************************************************************************************
		Connect the new client by creating a new client state object and adding it to the list:
		**************************************************************************************/
		
		{
		/* Lock the client state list: */
		Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
		
		try
			{
			/* Create a new client state object and add it to the list: */
			ClientState* newClientState=new ClientState(clientSocket);
			clientStates.push_back(newClientState);
			
			/* Start a communication thread for the new client: */
			newClientState->communicationThread.start(this,&SharedJelloServer::clientCommunicationThreadMethod,newClientState);
			}
		catch(std::runtime_error err)
			{
			std::cerr<<"SharedJelloServer: Cancelled connecting new client due to exception "<<err.what()<<std::endl<<std::flush;
			}
		catch(...)
			{
			std::cerr<<"SharedJelloServer: Cancelled connecting new client due to spurious exception"<<std::endl<<std::flush;
			}
		}
		}
	
	return 0;
	}

void* SharedJelloServer::clientCommunicationThreadMethod(SharedJelloServer::ClientState* clientState)
	{
	/* Enable immediate cancellation of this thread: */
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	SharedJelloPipe& pipe=clientState->pipe;
	
	try
		{
		/* Connect the client by sending the size of the Jell-O crystal: */
		{
		Threads::Mutex::Lock pipeLock(pipe.getMutex());
		pipe.writeMessage(SharedJelloPipe::CONNECT_REPLY);
		pipe.writePoint(crystal.getDomain().getMin());
		pipe.writePoint(crystal.getDomain().getMax());
		pipe.write<int>(crystal.getNumAtoms().getComponents(),3);
		pipe.flushWrite();
		}
		
		/* Mark the client as connected: */
		{
		Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
		clientState->connected=true;
		}
		
		#ifdef VERBOSE
		std::cout<<"SharedJelloServer: Connection to client from host "<<pipe.getPeerHostname()<<", port "<<pipe.getPeerPortId()<<" established"<<std::endl<<std::flush;
		#endif
		
		/* Run the client communication protocol machine: */
		while(true)
			{
			/* Wait for the next message: */
			SharedJelloPipe::MessageIdType message=pipe.readMessage();
			
			/* Bail out if a disconnect request or an unexpected message was received: */
			if(message==SharedJelloPipe::DISCONNECT_REQUEST)
				{
				pipe.flushRead();
				
				/* Send a disconnect reply: */
				{
				Threads::Mutex::Lock pipeLock(pipe.getMutex());
				pipe.writeMessage(SharedJelloPipe::DISCONNECT_REPLY);
				pipe.flushWrite();
				pipe.shutdown(false,true);
				}
				
				/* Bail out: */
				break;
				}
			else if(message==SharedJelloPipe::CLIENT_PARAMUPDATE)
				{
				/* Update the simulation parameter set: */
				{
				Threads::Mutex::Lock parameterLock(parameterMutex);
				++newParameterVersion;
				newAtomMass=pipe.read<Scalar>();
				newAttenuation=pipe.read<Scalar>();
				newGravity=pipe.read<Scalar>();
				}
				
				pipe.flushRead();
				}
			else if(message==SharedJelloPipe::CLIENT_UPDATE)
				{
				/* Lock the next free client update slot: */
				int nextIndex=(clientState->lockedIndex+1)%3;
				if(nextIndex==clientState->mostRecentIndex)
					nextIndex=(nextIndex+1)%3;
				ClientState::StateUpdate& su=clientState->stateUpdates[nextIndex];
				
				/* Process the client update message: */
				int newNumDraggers=pipe.read<int>();
				if(newNumDraggers!=su.numDraggers)
					{
					delete[] su.draggerIDs;
					delete[] su.draggerRayBaseds;
					delete[] su.draggerRays;
					delete[] su.draggerTransformations;
					delete[] su.draggerActives;
					su.numDraggers=newNumDraggers;
					if(su.numDraggers!=0)
						{
						su.draggerIDs=new unsigned int[su.numDraggers];
						su.draggerRayBaseds=new bool[su.numDraggers];
						su.draggerRays=new Ray[su.numDraggers];
						su.draggerTransformations=new ONTransform[su.numDraggers];
						su.draggerActives=new bool[su.numDraggers];
						}
					else
						{
						su.draggerIDs=0;
						su.draggerRayBaseds=0;
						su.draggerRays=0;
						su.draggerTransformations=0;
						su.draggerActives=0;
						}
					}
				
				for(int draggerIndex=0;draggerIndex<su.numDraggers;++draggerIndex)
					{
					su.draggerIDs[draggerIndex]=pipe.read<unsigned int>();
					su.draggerRayBaseds[draggerIndex]=pipe.read<int>()!=0;
					Point rayStart=pipe.readPoint();
					Vector rayDirection=pipe.readVector();
					su.draggerRays[draggerIndex]=Ray(rayStart,rayDirection);
					su.draggerTransformations[draggerIndex]=pipe.readONTransform();
					su.draggerActives[draggerIndex]=pipe.read<char>()!=char(0);
					}
				
				pipe.flushRead();
				
				/* Mark the client update slot as most recent: */
				clientState->mostRecentIndex=nextIndex;
				}
			else
				{
				pipe.flushRead();
				Misc::throwStdErr("Protocol error in client communication");
				}
			}
		}
	catch(std::runtime_error err)
		{
		/* Ignore any connection errors; just disconnect the client */
		std::cerr<<"SharedJelloServer: Disconnecting client due to exception "<<err.what()<<std::endl<<std::flush;
		}
	catch(...)
		{
		/* Ignore any connection errors; just disconnect the client */
		std::cerr<<"SharedJelloServer: Disconnecting client due to spurious exception"<<std::endl<<std::flush;
		}
	
	/******************************************************************************************
	Disconnect the client by removing it from the list and deleting the client state structure:
	******************************************************************************************/
	
	#ifdef VERBOSE
	std::cout<<"SharedJelloServer: Disconnecting client from host "<<pipe.getPeerHostname()<<", port "<<pipe.getPeerPortId()<<std::endl<<std::flush;
	#endif
	
	{
	/* Lock the client state list: */
	Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
	
	/* Unlock all atoms held by the client: */
	for(std::vector<ClientState::AtomLock>::iterator alIt=clientState->atomLocks.begin();alIt!=clientState->atomLocks.end();++alIt)
		crystal.unlockAtom(alIt->draggedAtom);
	
	/* Find this client's state in the list: */
	ClientStateList::iterator cslIt;
	for(cslIt=clientStates.begin();cslIt!=clientStates.end()&&*cslIt!=clientState;++cslIt)
		;
	
	/* Remove the client state from the list: */
	clientStates.erase(cslIt);
	
	/* Delete the client state object: */
	delete clientState;
	}
	
	return 0;
	}

SharedJelloServer::SharedJelloServer(const SharedJelloServer::Index& numAtoms,int listenPortID)
	:newParameterVersion(1),
	 crystal(numAtoms),
	 parameterVersion(1),
	 listenSocket(listenPortID,0)
	{
	/* Start listening thread: */
	listenThread.start(this,&SharedJelloServer::listenThreadMethod);
	}

SharedJelloServer::~SharedJelloServer(void)
	{
	{
	/* Lock client list: */
	Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
	
	/* Stop connection initiating thread: */
	listenThread.cancel();
	listenThread.join();
	
	/* Disconnect all clients: */
	for(ClientStateList::iterator cslIt=clientStates.begin();cslIt!=clientStates.end();++cslIt)
		{
		/* Stop client communication thread: */
		(*cslIt)->communicationThread.cancel();
		(*cslIt)->communicationThread.join();
		
		/* Delete client state object: */
		delete *cslIt;
		}
	}
	}

void SharedJelloServer::simulate(double timeStep)
	{
	{
	Threads::Mutex::Lock parameterLock(parameterMutex);
	if(newParameterVersion!=parameterVersion)
		{
		/* Update the Jell-O crystal's simulation parameters: */
		crystal.setAtomMass(newAtomMass);
		crystal.setAttenuation(newAttenuation);
		crystal.setGravity(newGravity);
		parameterVersion=newParameterVersion;
		}
	}
	
	/*******************************************************************************
	Process all client state updates received since the beginning of the last frame:
	*******************************************************************************/
	
	{
	/* Lock the client state list: */
	Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
	
	/* Check all client states for recent updates: */
	for(ClientStateList::iterator cslIt=clientStates.begin();cslIt!=clientStates.end();++cslIt)
		{
		/* Check if there has been an update since the last frame: */
		ClientState* cs=*cslIt;
		if(cs->lockedIndex!=cs->mostRecentIndex)
			{
			/* Lock the most recent update: */
			cs->lockedIndex=cs->mostRecentIndex;
			ClientState::StateUpdate* su=&cs->stateUpdates[cs->lockedIndex];
			
			/* Update the list of atoms locked by this client: */
			for(int draggerIndex=0;draggerIndex<su->numDraggers;++draggerIndex)
				{
				if(su->draggerActives[draggerIndex])
					{
					/* Check if this dragger has just become active: */
					std::vector<ClientState::AtomLock>::iterator alIt;
					for(alIt=cs->atomLocks.begin();alIt!=cs->atomLocks.end()&&alIt->draggerID!=su->draggerIDs[draggerIndex];++alIt)
						;
					if(alIt==cs->atomLocks.end())
						{
						/* Find the atom picked by the dragger: */
						ClientState::AtomLock al;
						al.draggerID=su->draggerIDs[draggerIndex];
						if(su->draggerRayBaseds[draggerIndex])
							al.draggedAtom=crystal.pickAtom(su->draggerRays[draggerIndex]);
						else
							al.draggedAtom=crystal.pickAtom(su->draggerTransformations[draggerIndex].getOrigin());
						
						/* Try locking the atom: */
						if(crystal.lockAtom(al.draggedAtom))
							{
							/* Calculate the dragging transformation: */
							al.dragTransformation=su->draggerTransformations[draggerIndex];
							al.dragTransformation.doInvert();
							al.dragTransformation*=crystal.getAtomState(al.draggedAtom);
							
							/* Store the atom lock in the list: */
							alIt=cs->atomLocks.insert(alIt,al);
							}
						}
					
					if(alIt!=cs->atomLocks.end())
						{
						/* Set the position/orientation of the locked atom: */
						ONTransform transform=su->draggerTransformations[draggerIndex];
						transform*=alIt->dragTransformation;
						crystal.setAtomState(alIt->draggedAtom,transform);
						}
					}
				else
					{
					/* Check if this dragger has just become inactive: */
					for(std::vector<ClientState::AtomLock>::iterator alIt=cs->atomLocks.begin();alIt!=cs->atomLocks.end();++alIt)
						if(alIt->draggerID==su->draggerIDs[draggerIndex])
							{
							/* Release the atom lock: */
							crystal.unlockAtom(alIt->draggedAtom);
							cs->atomLocks.erase(alIt);
							break;
							}
					}
				}
			}
		}
	}
	
	/* Simulate the crystal's behavior in this time step: */
	crystal.simulate(timeStep);
	}

void SharedJelloServer::sendServerUpdate(void)
	{
	{
	/* Lock the client state list: */
	Threads::Mutex::Lock clientStateListLock(clientStateListMutex);
	
	/* Go through all client states: */
	for(ClientStateList::iterator cslIt=clientStates.begin();cslIt!=clientStates.end();++cslIt)
		{
		ClientState* cs=*cslIt;
		if(cs->connected)
			{
			try
				{
				{
				Threads::Mutex::Lock pipeLock(cs->pipe.getMutex());
				
				if(cs->parameterVersion!=parameterVersion)
					{
					/* Send a parameter update message: */
					cs->pipe.writeMessage(SharedJelloPipe::SERVER_PARAMUPDATE);
					cs->pipe.write<Scalar>(crystal.getAtomMass());
					cs->pipe.write<Scalar>(crystal.getAttenuation());
					cs->pipe.write<Scalar>(crystal.getGravity());
					cs->parameterVersion=parameterVersion;
					}
				
				/* Send a server update message: */
				cs->pipe.writeMessage(SharedJelloPipe::SERVER_UPDATE);
				
				/* Send the crystal's state: */
				crystal.writeAtomStates(cs->pipe);
				
				cs->pipe.flushWrite();
				}
				}
			catch(Comm::TCPSocket::PipeError err)
				{
				/* Ignore the pipe error; let the client communication thread handle it */
				}
			catch(...)
				{
				/* Ignore the pipe error; let the client communication thread handle it */
				}
			}
		}
	}
	}

/*********************
Main program function:
*********************/

int main(int argc,char* argv[])
	{
	/* Parse the command line: */
	SharedJelloServer::Index numAtoms(4,4,8);
	int listenPortID=-1; // Assign any free port
	double updateTime=0.02; // Aim for 50 updates/sec
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"numAtoms")==0)
				{
				/* Read the number of atoms: */
				++i;
				for(int j=0;j<3&&i<argc;++j,++i)
					numAtoms[j]=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"port")==0)
				{
				/* Read the server listening port: */
				++i;
				if(i<argc)
					listenPortID=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"tick")==0)
				{
				/* Read the server update time interval: */
				++i;
				if(i<argc)
					updateTime=atof(argv[i]);
				}
			}
		}
	
	/* Ignore SIGPIPE and leave handling of pipe errors to TCP sockets: */
	struct sigaction sigPipeAction;
	sigPipeAction.sa_handler=SIG_IGN;
	sigemptyset(&sigPipeAction.sa_mask);
	sigPipeAction.sa_flags=0x0;
	sigaction(SIGPIPE,&sigPipeAction,0);
	
	/* Create a shared Jell-O server: */
	SharedJelloServer sjs(numAtoms,listenPortID);
	std::cout<<"SharedJelloServer::main: Created Jell-O server listening on port "<<sjs.getListenPortID()<<std::endl<<std::flush;
	
	/* Run the simulation loop full speed: */
	Misc::Timer timer;
	double lastFrameTime=timer.peekTime();
	double nextUpdateTime=timer.peekTime()+updateTime;
	int numFrames=0;
	while(true)
		{
		/* Calculate the current time step duration: */
		double newFrameTime=timer.peekTime();
		double timeStep=newFrameTime-lastFrameTime;
		lastFrameTime=newFrameTime;
		
		/* Perform a simulation step: */
		sjs.simulate(timeStep);
		++numFrames;
		
		/* Check if it's time for a state update: */
		if(lastFrameTime>=nextUpdateTime)
			{
			/* Send a state update to all connected clients: */
			sjs.sendServerUpdate();
			
			// std::cout<<"\rFrame rate: "<<double(numFrames)/updateTime<<" fps"<<std::flush;
			nextUpdateTime+=updateTime;
			numFrames=0;
			}
		}
	
	return 0;
	}
