/***********************************************************************
DeviceTest - Program to test the connection to a Vrui VR Device Daemon
and to dump device positions/orientations and button states.
Copyright (c) 2002-2017 Oliver Kreylos

This file is part of the Virtual Reality User Interface Library (Vrui).

The Virtual Reality User Interface Library is free software; you can
redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

The Virtual Reality User Interface Library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Virtual Reality User Interface Library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA
***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <Misc/SizedTypes.h>
#include <Misc/Timer.h>
#include <Misc/FunctionCalls.h>
#include <Misc/Marshaller.h>
#include <Geometry/GeometryMarshallers.h>
#include <Misc/ConfigurationFile.h>
#include <IO/File.h>
#include <IO/OpenFile.h>
#include <Realtime/Time.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/OutputOperators.h>
#include <Vrui/Internal/VRDeviceDescriptor.h>
#include <Vrui/Internal/BatteryState.h>
#include <Vrui/Internal/HMDConfiguration.h>
#include <Vrui/Internal/VRDeviceClient.h>

typedef Vrui::VRDeviceState::TrackerState TrackerState;
typedef TrackerState::PositionOrientation PositionOrientation;
typedef PositionOrientation::Scalar Scalar;
typedef PositionOrientation::Point Point;
typedef PositionOrientation::Vector Vector;
typedef PositionOrientation::Rotation Rotation;

class LatencyHistogram // Helper class to collect and print tracker data latency histograms
	{
	/* Elements: */
	private:
	unsigned int binSize; // Size of a histogram bin in microseconds
	unsigned int maxBinLatency; // Maximum latency to expect in microseconds
	unsigned int numBins; // Number of bins in the histogram
	unsigned int* bins; // Array of histogram bins
	unsigned int numSamples; // Number of samples in current observation period
	double latencySum; // Sum of all latencies to calculate average latency
	unsigned int minLatency,maxLatency; // Latency range in current observation period in microseconds
	unsigned int maxBinSize; // Maximum number of samples in any bin
	
	/* Constructors and destructors: */
	public:
	LatencyHistogram(unsigned int sBinSize,unsigned int sMaxBinLatency)
		:binSize(sBinSize),maxBinLatency(sMaxBinLatency),
		 numBins(maxBinLatency/binSize+2),bins(new unsigned int[numBins])
		{
		/* Initialize the histogram: */
		reset();
		}
	~LatencyHistogram(void)
		{
		delete[] bins;
		}
	
	/* Methods: */
	void reset(void) // Resets the histogram for the next observation period
		{
		/* Clear the histogram: */
		for(unsigned int i=0;i<numBins;++i)
			bins[i]=0;
		
		/* Reset the latency counter and range: */
		numSamples=0;
		latencySum=0.0;
		minLatency=~0x0U;
		maxLatency=0U;
		maxBinSize=0U;
		}
	void addSample(unsigned int latency) // Adds a latency sample
		{
		/* Update the histogram: */
		unsigned int binIndex=latency/binSize;
		if(binIndex>numBins-1)
			binIndex=numBins-1; // All outliers go into the last bin
		++bins[binIndex];
		if(maxBinSize<bins[binIndex])
			maxBinSize=bins[binIndex];
		
		/* Update sample counter and range: */
		++numSamples;
		latencySum+=double(latency);
		if(minLatency>latency)
			minLatency=latency;
		if(maxLatency<latency)
			maxLatency=latency;
		}
	unsigned int getNumSamples(void) const
		{
		return numSamples;
		}
	void printHistogram(void) const // Prints the histogram
		{
		/* Calculate the range of non-empty bins: */
		unsigned int firstBinIndex=minLatency/binSize;
		if(firstBinIndex>numBins-1)
			firstBinIndex=numBins-1;
		unsigned int lastBinIndex=maxLatency/binSize;
		if(lastBinIndex>numBins-1)
			lastBinIndex=numBins-1;
		
		std::cout<<"Histogram of "<<numSamples<<" latency samples:"<<std::endl;
		for(unsigned int i=firstBinIndex;i<=lastBinIndex;++i)
			{
			if(i<numBins-1)
				std::cout<<std::setw(8)<<i*binSize<<' ';
			else
				std::cout<<"Outliers ";
			unsigned int width=(bins[i]*71+maxBinSize-1)/maxBinSize;
			for(unsigned int j=0;j<width;++j)
				std::cout<<'*';
			std::cout<<std::endl;
			}
		
		std::cout<<"Average latency: "<<latencySum/double(numSamples)<<" us"<<std::endl;
		}
	};

void printTrackerPos(const Vrui::VRDeviceState& state,int trackerIndex)
	{
	if(state.getTrackerValid(trackerIndex))
		{
		const TrackerState& ts=state.getTrackerState(trackerIndex);
		Point pos=ts.positionOrientation.getOrigin();
		std::cout.setf(std::ios::fixed);
		std::cout.precision(3);
		std::cout<<"("<<std::setw(9)<<pos[0]<<" "<<std::setw(9)<<pos[1]<<" "<<std::setw(9)<<pos[2]<<")";
		}
	else
		{
		std::cout<<"(-----.--- -----.--- -----.---)";
		}
	}

void printTrackerPosOrient(const Vrui::VRDeviceState& state,int trackerIndex)
	{
	if(state.getTrackerValid(trackerIndex))
		{
		const TrackerState& ts=state.getTrackerState(trackerIndex);
		Point pos=ts.positionOrientation.getOrigin();
		Rotation rot=ts.positionOrientation.getRotation();
		Vector axis=rot.getScaledAxis();
		Scalar angle=Math::deg(rot.getAngle());
		std::cout.setf(std::ios::fixed);
		std::cout.precision(3);
		std::cout<<"("<<std::setw(8)<<pos[0]<<" "<<std::setw(8)<<pos[1]<<" "<<std::setw(8)<<pos[2]<<") ";
		std::cout<<"("<<std::setw(8)<<axis[0]<<" "<<std::setw(8)<<axis[1]<<" "<<std::setw(8)<<axis[2]<<") ";
		std::cout<<std::setw(8)<<angle;
		}
	else
		{
		std::cout<<"(----.--- ----.--- ----.---) (----.--- ----.--- ----.---) ----.---";
		}
	}

void printTrackerFrame(const Vrui::VRDeviceState& state,int trackerIndex)
	{
	if(state.getTrackerValid(trackerIndex))
		{
		const TrackerState& ts=state.getTrackerState(trackerIndex);
		Point pos=ts.positionOrientation.getOrigin();
		Rotation rot=ts.positionOrientation.getRotation();
		Vector x=rot.getDirection(0);
		Vector y=rot.getDirection(1);
		Vector z=rot.getDirection(2);
		std::cout.setf(std::ios::fixed);
		std::cout.precision(3);
		std::cout<<"("<<std::setw(8)<<pos[0]<<" "<<std::setw(8)<<pos[1]<<" "<<std::setw(8)<<pos[2]<<") ";
		std::cout<<"("<<std::setw(6)<<x[0]<<" "<<std::setw(6)<<x[1]<<" "<<std::setw(6)<<x[2]<<") ";
		std::cout<<"("<<std::setw(6)<<y[0]<<" "<<std::setw(6)<<y[1]<<" "<<std::setw(6)<<y[2]<<") ";
		std::cout<<"("<<std::setw(6)<<z[0]<<" "<<std::setw(6)<<z[1]<<" "<<std::setw(6)<<z[2]<<")";
		}
	else
		{
		std::cout<<"(----.--- ----.--- ----.---) ";
		std::cout<<"(--.--- --.--- --.---) ";
		std::cout<<"(--.--- --.--- --.---) ";
		std::cout<<"(--.--- --.--- --.---)";
		}
	}

void printButtons(const Vrui::VRDeviceState& state)
	{
	for(int i=0;i<state.getNumButtons();++i)
		{
		if(i>0)
			std::cout<<" ";
		if(state.getButtonState(i))
			std::cout<<"X";
		else
			std::cout<<'.';
		}
	}

void printValuators(const Vrui::VRDeviceState& state)
	{
	std::cout.setf(std::ios::fixed);
	std::cout.precision(3);
	for(int i=0;i<state.getNumValuators();++i)
		{
		if(i>0)
			std::cout<<" ";
		std::cout<<std::setw(6)<<state.getValuatorState(i);
		}
	}

unsigned int numHmdConfigurations=0;
const Vrui::HMDConfiguration** hmdConfigurations=0;
unsigned int* eyePosVersions=0;
unsigned int* eyeVersions=0;
unsigned int* distortionMeshVersions=0;

void hmdConfigurationUpdatedCallback(const Vrui::HMDConfiguration& hmdConfiguration)
	{
	/* Find the updated HMD configuration in the list: */
	unsigned int index;
	for(index=0;index<numHmdConfigurations&&hmdConfigurations[index]!=&hmdConfiguration;++index)
		;
	if(index<numHmdConfigurations)
		{
		std::cout<<"Received configuration update for HMD "<<index<<std::endl;
		if(eyePosVersions[index]!=hmdConfiguration.getEyePosVersion())
			{
			std::cout<<"  Updated left eye position : "<<hmdConfiguration.getEyePosition(0)<<std::endl;
			std::cout<<"  Updated right eye position: "<<hmdConfiguration.getEyePosition(0)<<std::endl;
			
			eyePosVersions[index]=hmdConfiguration.getEyePosVersion();
			}
		if(eyeVersions[index]!=hmdConfiguration.getEyeVersion())
			{
			std::cout<<"  Updated left eye field-of-view : "<<hmdConfiguration.getFov(0)[0]<<", "<<hmdConfiguration.getFov(0)[1]<<", "<<hmdConfiguration.getFov(0)[2]<<", "<<hmdConfiguration.getFov(0)[3]<<std::endl;
			std::cout<<"  Updated right eye field-of-view: "<<hmdConfiguration.getFov(1)[0]<<", "<<hmdConfiguration.getFov(1)[1]<<", "<<hmdConfiguration.getFov(1)[2]<<", "<<hmdConfiguration.getFov(1)[3]<<std::endl;
			
			eyeVersions[index]=hmdConfiguration.getEyeVersion();
			}
		if(distortionMeshVersions[index]!=hmdConfiguration.getDistortionMeshVersion())
			{
			std::cout<<"  Updated render target size: "<<hmdConfiguration.getRenderTargetSize()[0]<<" x "<<hmdConfiguration.getRenderTargetSize()[1]<<std::endl;
			std::cout<<"  Updated distortion mesh size: "<<hmdConfiguration.getDistortionMeshSize()[0]<<" x "<<hmdConfiguration.getDistortionMeshSize()[1]<<std::endl;
			std::cout<<"  Updated left eye viewport : "<<hmdConfiguration.getViewport(0)[0]<<", "<<hmdConfiguration.getViewport(0)[1]<<", "<<hmdConfiguration.getViewport(0)[2]<<", "<<hmdConfiguration.getViewport(0)[3]<<std::endl;
			std::cout<<"  Updated right eye viewport: "<<hmdConfiguration.getViewport(1)[0]<<", "<<hmdConfiguration.getViewport(1)[1]<<", "<<hmdConfiguration.getViewport(1)[2]<<", "<<hmdConfiguration.getViewport(1)[3]<<std::endl;
			
			distortionMeshVersions[index]=hmdConfiguration.getDistortionMeshVersion();
			}
		}
	}

int main(int argc,char* argv[])
	{
	/* Parse command line: */
	const char* serverNamePort="localhost:8555";
	bool printDevices=false;
	bool printHmdConfigurations=false;
	int trackerIndex=0;
	int printMode=0;
	bool printButtonStates=false;
	bool printNewlines=false;
	bool savePositions=false;
	bool saveTrackerStates=false;
	const char* saveFileName=0;
	int triggerIndex=0;
	int latencyIndex=-1;
	unsigned int latencyBinSize=250;
	unsigned int latencyMaxLatency=20000;
	unsigned int latencyNumSamples=1000;
	int powerFeatureIndex=-1;
	int hapticFeatureIndex=-1;
	unsigned int hapticDuration=0;
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i],"-listDevices")==0||strcasecmp(argv[i],"-ld")==0)
				printDevices=true;
			else if(strcasecmp(argv[i],"-listHMDs")==0||strcasecmp(argv[i],"-lh")==0)
				printHmdConfigurations=true;
			else if(strcasecmp(argv[i],"-t")==0||strcasecmp(argv[i],"--trackerIndex")==0)
				{
				++i;
				trackerIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i],"-alltrackers")==0)
				trackerIndex=-1;
			else if(strcasecmp(argv[i],"-p")==0)
				printMode=0;
			else if(strcasecmp(argv[i],"-o")==0)
				printMode=1;
			else if(strcasecmp(argv[i],"-f")==0)
				printMode=2;
			else if(strcasecmp(argv[i],"-v")==0)
				printMode=3;
			else if(strcasecmp(argv[i],"-b")==0)
				printButtonStates=true;
			else if(strcasecmp(argv[i],"-n")==0)
				printNewlines=true;
			else if(strcasecmp(argv[i],"-save")==0)
				{
				savePositions=true;
				++i;
				saveFileName=argv[i];
				}
			else if(strcasecmp(argv[i],"-saveTs")==0)
				{
				saveTrackerStates=true;
				++i;
				saveFileName=argv[i];
				}
			else if(strcasecmp(argv[i],"-trigger")==0)
				{
				++i;
				triggerIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i],"-latency")==0)
				{
				++i;
				latencyIndex=atoi(argv[i]);
				++i;
				latencyBinSize=(unsigned int)(atoi(argv[i]));
				++i;
				latencyMaxLatency=(unsigned int)(atoi(argv[i]));
				++i;
				latencyNumSamples=(unsigned int)(atoi(argv[i]));
				}
			else if(strcasecmp(argv[i],"-poweroff")==0)
				{
				++i;
				powerFeatureIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i],"-haptic")==0)
				{
				++i;
				hapticFeatureIndex=atoi(argv[i]);
				++i;
				hapticDuration=atoi(argv[i]);
				}
			}
		else
			serverNamePort=argv[i];
		}
	
	if(serverNamePort==0)
		{
		std::cerr<<"Usage: "<<argv[0]<<" [-ld | -listDevices] [-lh | -listHMDs] [(-t | --trackerIndex) <trackerIndex>] [-alltrackers] [-p | -o | -f | -v] [-b] [-n] [-save <save file name>] [-trigger <trigger index>] [-latency <trackerIndex> <bin size> <max latency> <num samples>] <serverName:serverPort>"<<std::endl;
		return 1;
		}
	
	/* Split the server name into hostname:port: */
	const char* colonPtr=0;
	for(const char* cPtr=serverNamePort;*cPtr!='\0';++cPtr)
		if(*cPtr==':')
			colonPtr=cPtr;
	std::string serverName;
	int portNumber=8555;
	if(colonPtr!=0)
		{
		serverName=std::string(serverNamePort,colonPtr);
		portNumber=atoi(colonPtr+1);
		}
	else
		serverName=serverNamePort;
	
	/* Initialize device client: */
	Vrui::VRDeviceClient* deviceClient=0;
	try
		{
		deviceClient=new Vrui::VRDeviceClient(serverName.c_str(),portNumber);
		}
	catch(std::runtime_error error)
		{
		std::cerr<<"Caught exception "<<error.what()<<" while initializing VR device client"<<std::endl;
		return 1;
		}
	if(deviceClient->isLocal())
		std::cout<<"Device server at "<<serverName<<':'<<portNumber<<" is running on same host"<<std::endl;
	
	if(printDevices)
		{
		/* Print information about the server's virtual input devices: */
		std::cout<<"Device server at "<<serverName<<':'<<portNumber<<" defines "<<deviceClient->getNumVirtualDevices()<<" virtual input devices."<<std::endl;
		for(int deviceIndex=0;deviceIndex<deviceClient->getNumVirtualDevices();++deviceIndex)
			{
			const Vrui::VRDeviceDescriptor& vd=deviceClient->getVirtualDevice(deviceIndex);
			std::cout<<"Virtual device "<<vd.name<<":"<<std::endl;
			std::cout<<"  Track type: ";
			if(vd.trackType&Vrui::VRDeviceDescriptor::TRACK_ORIENT)
				std::cout<<"6-DOF";
			else if(vd.trackType&Vrui::VRDeviceDescriptor::TRACK_DIR)
				std::cout<<"Ray-based";
			else if(vd.trackType&Vrui::VRDeviceDescriptor::TRACK_POS)
				std::cout<<"3-DOF";
			else
				std::cout<<"None";
			std::cout<<std::endl;
			
			if(vd.trackType&Vrui::VRDeviceDescriptor::TRACK_DIR)
				std::cout<<"  Device ray direction: "<<vd.rayDirection<<", start: "<<vd.rayStart<<std::endl;
			
			std::cout<<"  Device is "<<(vd.hasBattery?"battery-powered":"connected to power source")<<std::endl;
			
			std::cout<<"  Device can ";
			if(vd.canPowerOff)
				std::cout<<"not ";
			std::cout<<"be powered off on request"<<std::endl;
			
			if(vd.trackType&Vrui::VRDeviceDescriptor::TRACK_POS)
				std::cout<<"  Tracker index: "<<vd.trackerIndex<<std::endl;
			
			if(vd.numButtons>0)
				{
				std::cout<<"  "<<vd.numButtons<<" buttons:";
				for(int i=0;i<vd.numButtons;++i)
					std::cout<<" ("<<vd.buttonNames[i]<<", "<<vd.buttonIndices[i]<<")";
				std::cout<<std::endl;
				}
			
			if(vd.numValuators>0)
				{
				std::cout<<"  "<<vd.numValuators<<" valuators:";
				for(int i=0;i<vd.numValuators;++i)
					std::cout<<" ("<<vd.valuatorNames[i]<<", "<<vd.valuatorIndices[i]<<")";
				std::cout<<std::endl;
				}
			
			if(vd.numHapticFeatures>0)
				{
				std::cout<<"  "<<vd.numHapticFeatures<<" haptic features:";
				for(int i=0;i<vd.numHapticFeatures;++i)
					std::cout<<" ("<<vd.hapticFeatureNames[i]<<", "<<vd.hapticFeatureIndices[i]<<")";
				std::cout<<std::endl;
				}
			}
		std::cout<<std::endl;
		}
	
	if(printHmdConfigurations)
		{
		/* Print information about the server's HMD configurations: */
		std::cout<<"Device server at "<<serverName<<':'<<portNumber<<" defines "<<deviceClient->getNumHmdConfigurations()<<" head-mounted devices."<<std::endl;
		deviceClient->lockHmdConfigurations();
		for(unsigned int hmdIndex=0;hmdIndex<deviceClient->getNumHmdConfigurations();++hmdIndex)
			{
			const Vrui::HMDConfiguration& hc=deviceClient->getHmdConfiguration(hmdIndex);
			std::cout<<"Head-mounted device "<<hmdIndex<<":"<<std::endl;
			std::cout<<"  Tracker index: "<<hc.getTrackerIndex()<<std::endl;
			std::cout<<"  Left eye position : "<<hc.getEyePosition(0)<<std::endl;
			std::cout<<"  Right eye position: "<<hc.getEyePosition(1)<<std::endl;
			std::cout<<"  Recommended per-eye render target size: "<<hc.getRenderTargetSize()[0]<<" x "<<hc.getRenderTargetSize()[1]<<std::endl;
			std::cout<<"  Per-eye distortion mesh size: "<<hc.getDistortionMeshSize()[0]<<" x "<<hc.getDistortionMeshSize()[1]<<std::endl;
			std::cout<<"  Left eye display viewport : "<<hc.getViewport(0)[0]<<", "<<hc.getViewport(0)[1]<<", "<<hc.getViewport(0)[2]<<", "<<hc.getViewport(0)[3]<<std::endl;
			std::cout<<"  Right eye display viewport: "<<hc.getViewport(1)[0]<<", "<<hc.getViewport(1)[1]<<", "<<hc.getViewport(1)[2]<<", "<<hc.getViewport(1)[3]<<std::endl;
			std::cout<<"  Left eye field-of-view : "<<hc.getFov(0)[0]<<", "<<hc.getFov(0)[1]<<", "<<hc.getFov(0)[2]<<", "<<hc.getFov(0)[3]<<std::endl;
			std::cout<<"  Right eye field-of-view: "<<hc.getFov(1)[0]<<", "<<hc.getFov(1)[1]<<", "<<hc.getFov(1)[2]<<", "<<hc.getFov(1)[3]<<std::endl;
			}
		deviceClient->unlockHmdConfigurations();
		}
	
	/* Check whether to trigger a haptic pulse: */
	if(powerFeatureIndex>=0||hapticFeatureIndex>=0)
		{
		/* Request a power off or haptic tick and disconnect from the server: */
		try
			{
			deviceClient->activate();
			if(hapticFeatureIndex>=0)
				deviceClient->hapticTick(hapticFeatureIndex,hapticDuration);
			if(powerFeatureIndex>=0)
				deviceClient->powerOff(powerFeatureIndex);
			deviceClient->deactivate();
			}
		catch(std::runtime_error err)
			{
			std::cerr<<"Caught exception "<<err.what()<<" while powering off device / triggering haptic pulse"<<std::endl;
			}
		delete deviceClient;
		
		return 0;
		}
	
	/* Initialize HMD configuration state arrays: */
	deviceClient->lockHmdConfigurations();
	numHmdConfigurations=deviceClient->getNumHmdConfigurations();
	hmdConfigurations=new const Vrui::HMDConfiguration*[numHmdConfigurations];
	eyePosVersions=new unsigned int[numHmdConfigurations];
	eyeVersions=new unsigned int[numHmdConfigurations];
	distortionMeshVersions=new unsigned int[numHmdConfigurations];
	for(unsigned int i=0;i<numHmdConfigurations;++i)
		{
		hmdConfigurations[i]=&deviceClient->getHmdConfiguration(i);
		eyePosVersions[i]=hmdConfigurations[i]->getEyePosVersion();
		eyeVersions[i]=hmdConfigurations[i]->getEyeVersion();
		distortionMeshVersions[i]=hmdConfigurations[i]->getDistortionMeshVersion();
		deviceClient->setHmdConfigurationUpdatedCallback(hmdConfigurations[i]->getTrackerIndex(),Misc::createFunctionCall(hmdConfigurationUpdatedCallback));
		}
	deviceClient->unlockHmdConfigurations();
	
	/* Disable printing of tracking information if there are no trackers: */
	deviceClient->lockState();
	if(printMode>=0&&printMode<3&&deviceClient->getState().getNumTrackers()==0)
		printMode=-1;
	deviceClient->unlockState();
	
	/* Find the index of the virtual device to which the selected tracker belongs and check whether it's battery-powered: */
	int vdIndex=-1;
	bool vdHasBattery=false;
	for(int deviceIndex=0;deviceIndex<deviceClient->getNumVirtualDevices()&&vdIndex<0;++deviceIndex)
		{
		const Vrui::VRDeviceDescriptor& vd=deviceClient->getVirtualDevice(deviceIndex);
		if(vd.trackerIndex==trackerIndex)
			{
			vdIndex=deviceIndex;
			vdHasBattery=vd.hasBattery;
			}
		}
	
	/* Open the save file: */
	FILE* saveFile=0;
	IO::FilePtr saveTsFile=0;
	Vrui::VRDeviceState::TimeStamp lastTsTs=0;
	if(savePositions)
		saveFile=fopen(saveFileName,"wt");
	else if(saveTrackerStates)
		saveTsFile=IO::openFile(saveFileName,IO::File::WriteOnly);
	
	/* Print output header line: */
	switch(printMode)
		{
		case 0:
			std::cout<<"     Pos X     Pos Y     Pos Z";
			break;
		
		case 1:
			std::cout<<"    Pos X    Pos Y    Pos Z     Axis X   Axis Y   Axis Z     Angle";
			break;
		
		case 2:
			std::cout<<"    Pos X    Pos Y    Pos Z     XA X   XA Y   XA Z     YA X   YA Y   YA Z     ZA X   ZA Y   ZA Z";
			break;
		}
	if(vdHasBattery)
		std::cout<<"  Battr.";
	std::cout<<std::endl;
	
	LatencyHistogram* latencyHistogram=0;
	if(latencyIndex>=0)
		{
		/* Create a latency histogram: */
		latencyHistogram=new LatencyHistogram(latencyBinSize,latencyMaxLatency);
		}
	
	/* Run main loop: */
	Misc::Timer t;
	int numPackets=0;
	try
		{
		deviceClient->activate();
		deviceClient->startStream(0);
		
		bool loop=true;
		bool oldTriggerState=false;
		while(loop)
			{
			/* Get packet timestamp: */
			Realtime::TimePointMonotonic now;
			Vrui::VRDeviceState::TimeStamp nowTs=Vrui::VRDeviceState::TimeStamp(now.tv_sec*1000000+(now.tv_nsec+500)/1000);
			++numPackets;
			
			/* Print new device state: */
			if(!printNewlines)
				std::cout<<"\r";
			deviceClient->lockState();
			const Vrui::VRDeviceState& state=deviceClient->getState();
			
			if(latencyHistogram!=0)
				{
				latencyHistogram->addSample(nowTs-state.getTrackerTimeStamp(latencyIndex));
				if(latencyHistogram->getNumSamples()>=latencyNumSamples)
					{
					latencyHistogram->printHistogram();
					latencyHistogram->reset();
					}
				}
			
			if(savePositions)
				{
				if(oldTriggerState==false&&state.getButtonState(triggerIndex))
					{
					/* Save the current position: */
					Point::AffineCombiner pc;
					for(int i=0;i<50;++i)
						{
						/* Accumulate the current position: */
						const TrackerState& ts=state.getTrackerState(trackerIndex);
						pc.addPoint(ts.positionOrientation.getOrigin());

						/* Wait for the next packet: */
						deviceClient->unlockState();
						deviceClient->getPacket();
						deviceClient->lockState();
						}

					/* Save the accumulated position: */
					Point p=pc.getPoint();
					fprintf(saveFile,"%14.8f %14.8f %14.8f\n",p[0],p[1],p[2]);
					}
				oldTriggerState=state.getButtonState(triggerIndex);
				}
			else if(saveTrackerStates&&state.getButtonState(triggerIndex))
				{
				/* Check if the tracked tracker has a new tracking state: */
				if(lastTsTs!=state.getTrackerTimeStamp(trackerIndex))
					{
					/* Save the tracker's time stamp and state: */
					lastTsTs=state.getTrackerTimeStamp(trackerIndex);
					saveTsFile->write<Misc::SInt32>(lastTsTs);
					const TrackerState& ts=state.getTrackerState(trackerIndex);
					Misc::Marshaller<TrackerState::PositionOrientation>::write(ts.positionOrientation,*saveTsFile);
					Misc::Marshaller<TrackerState::LinearVelocity>::write(ts.linearVelocity,*saveTsFile);
					Misc::Marshaller<TrackerState::AngularVelocity>::write(ts.angularVelocity,*saveTsFile);
					}
				}
			
			switch(printMode)
				{
				case 0:
					if(trackerIndex<0)
						{
						printTrackerPos(state,0);
						for(int i=1;i<state.getNumTrackers();++i)
							{
							std::cout<<" ";
							printTrackerPos(state,i);
							}
						}
					else
						printTrackerPos(state,trackerIndex);
					break;

				case 1:
					printTrackerPosOrient(state,trackerIndex);
					break;

				case 2:
					printTrackerFrame(state,trackerIndex);
					break;

				case 3:
					printValuators(state);
					break;

				default:
					; // Print nothing; nothing, I say!
				}
			if(vdHasBattery)
				{
				deviceClient->lockBatteryStates();
				const Vrui::BatteryState& bs=deviceClient->getBatteryState(vdIndex);
				std::cout<<' '<<(bs.charging?"C ":"  ")<<std::setw(3)<<bs.batteryLevel<<'%';
				deviceClient->unlockBatteryStates();
				}
			if(printButtonStates)
				{
				std::cout<<" ";
				printButtons(state);
				}
			deviceClient->unlockState();
			if(printNewlines)
				std::cout<<std::endl;
			else
				std::cout<<std::flush;

			/* Check for a key press event: */
			fd_set readFdSet;
			FD_ZERO(&readFdSet);
			FD_SET(fileno(stdin),&readFdSet);
			struct timeval timeout;
			timeout.tv_sec=0;
			timeout.tv_usec=0;
			bool dataWaiting=select(fileno(stdin)+1,&readFdSet,0,0,&timeout)>=0&&FD_ISSET(fileno(stdin),&readFdSet);
			if(dataWaiting)
				loop=false;
			
			/* Wait for next packet: */
			deviceClient->getPacket();
			}
		std::cout<<std::endl;
		}
	catch(std::runtime_error err)
		{
		if(!printNewlines)
			std::cout<<std::endl;
		std::cerr<<"Caught exception "<<err.what()<<" while reading tracking data"<<std::endl;
		}
	t.elapse();
	std::cout<<"Received "<<numPackets<<" device data packets in "<<t.getTime()*1000.0<<" ms ("<<double(numPackets)/t.getTime()<<" packets/s)"<<std::endl;
	deviceClient->stopStream();
	deviceClient->deactivate();
	
	/* Clean up and terminate: */
	delete[] hmdConfigurations;
	delete[] eyePosVersions;
	delete[] eyeVersions;
	delete[] distortionMeshVersions;
	delete latencyHistogram;
	if(savePositions!=0)
		fclose(saveFile);
	else if(saveTrackerStates)
		saveTsFile=0;
	delete deviceClient;
	return 0;
	}
