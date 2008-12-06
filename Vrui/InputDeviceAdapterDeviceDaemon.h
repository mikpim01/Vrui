/***********************************************************************
InputDeviceAdapterDeviceDaemon - Class to convert from Vrui's own
distributed device driver architecture to Vrui's internal device
representation.
Copyright (c) 2004-2005 Oliver Kreylos

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

#ifndef VRUI_INPUTDEVICEADAPTERDEVICEDAEMON_INCLUDED
#define VRUI_INPUTDEVICEADAPTERDEVICEDAEMON_INCLUDED

#include <Vrui/VRDeviceClient.h>
#include <Vrui/InputDeviceAdapterIndexMap.h>

/* Forward declarations: */
namespace Misc {
class ConfigurationFileSection;
}

namespace Vrui {

class InputDeviceAdapterDeviceDaemon:public InputDeviceAdapterIndexMap
	{
	/* Elements: */
	private:
	VRDeviceClient deviceClient; // Device client delivering "raw" device state
	
	/* Private methods: */
	static void packetNotificationCallback(VRDeviceClient* client,void* userData);
	
	/* Constructors and destructors: */
	public:
	InputDeviceAdapterDeviceDaemon(InputDeviceManager* sInputDeviceManager,const Misc::ConfigurationFileSection& configFileSection); // Creates adapter by connecting to server and initializing Vrui input devices
	virtual ~InputDeviceAdapterDeviceDaemon(void);
	
	/* Methods: */
	virtual void updateInputDevices(void);
	};

}

#endif
