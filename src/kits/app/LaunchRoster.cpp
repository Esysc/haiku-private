/*
 * Copyright 2015 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include <LaunchRoster.h>

#include <String.h>

#include <LaunchDaemonDefs.h>
#include <MessengerPrivate.h>


using namespace BPrivate;


const BLaunchRoster* be_launch_roster;


BLaunchRoster::BLaunchRoster()
{
	_InitMessenger();
}


BLaunchRoster::~BLaunchRoster()
{
}


status_t
BLaunchRoster::InitCheck() const
{
	return fMessenger.Team() >= 0 ? B_OK : B_ERROR;
}


status_t
BLaunchRoster::GetData(const char* signature, BMessage& data)
{
	if (signature == NULL || signature[0] == '\0')
		return B_BAD_VALUE;

	BMessage request(B_GET_LAUNCH_DATA);
	status_t status = request.AddString("name", signature);
	if (status != B_OK)
		return status;

	// send the request
	status = fMessenger.SendMessage(&request, &data);

	// evaluate the reply
	if (status == B_OK)
		status = data.GetInt32("error", B_OK);

	return status;
}


port_id
BLaunchRoster::GetPort(const char* signature, const char* name)
{
	BLaunchRoster launchRoster;
	BMessage data;
	status_t status = launchRoster.GetData(signature, data);
	if (status == B_OK) {
		BString fieldName;
		if (name == NULL)
			fieldName = "port";
		else {
			fieldName = name;
			fieldName << "_port";
		}
		port_id port = data.GetInt32(fieldName.String(), B_NAME_NOT_FOUND);
		if (port >= 0)
			return port;
	}

	return -1;
}


void
BLaunchRoster::_InitMessenger()
{
	// find the launch_daemon port
	port_id daemonPort = find_port(B_LAUNCH_DAEMON_PORT_NAME);
	port_info info;
	if (daemonPort >= 0 && get_port_info(daemonPort, &info) == B_OK) {
		BMessenger::Private(fMessenger).SetTo(info.team, daemonPort,
			B_PREFERRED_TOKEN);
	}
}