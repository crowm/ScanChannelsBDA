/**
 *	main.cpp
 *	Copyright (C) 2004 BionicDonkey
 *  Copyright (C) 2004 Nate
 *
 *	This file is part of DigitalWatch, a free DTV watching and recording
 *	program for the VisionPlus DVB-T.
 *
 *	DigitalWatch is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	DigitalWatch is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with DigitalWatch; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stdafx.h"
#include "ScanChannelsBDA.h"
#include "FilterGraphTools.h"

HRESULT ShowMenu();
HRESULT GetChannelInfo(long &freq, long &band);

int	_tmain(int argc, _TCHAR* argv[])
{
	HRESULT hr;

	hr = ShowMenu();

	return 0;
}

HRESULT ShowMenu()
{
	HRESULT hr = S_OK;

	BOOL bExit = FALSE ;
	int iMenuSelect = 0;

	cout << "This program was designed to create channels.ini entries" << endl;
	cout << "for DigitalWatch using BDA drivers." << endl;
	cout << endl;
	cout << "Usage:" << endl;
	cout << "  Use 'Add Network' to add each frequency for your area." << endl;
	cout << "  eg." << endl;
	cout << "    Please Select Menu(1,2,5,6,10):1" << endl;
	cout << "    Frequency (KHz):177500" << endl;
	cout << "    BandWidth (Mbps):7" << endl;
	cout << endl;
	cout << "  Then use option 2 to generate the channel listing." << endl;
	cout << "  Once finished you can cut and paste the entries into your channels.ini file." << endl;
	cout << endl;
	cout << "Notes:" << endl;
	cout << "- If a channel has multiple audio pids then it will be listed multiple times." << endl;
	cout << "- If an AC3 PID is detected then an A will be prefixed to the audio pid." << endl;
	cout << "- If an AC3 PID is detected and is not the first pid for the program then the" << endl;
	cout << "  name will be suffixed with AC3" << endl << endl << endl;



	BDAChannelScan* cBDAChannelScan = new BDAChannelScan();

	if (FAILED(hr = cBDAChannelScan->selectCard()))
	{
		return 1;
	}
	if (hr != S_OK)
		return 0;
	
	if (FAILED(hr = cBDAChannelScan->CreateGraph()))
	{
		cout << "Failed to create graph." << endl;
		return 1;
	}
	
	if (FAILED(hr = cBDAChannelScan->BuildGraph()))
	{
		cout << "Failed to add filters to graph." << endl;
		return 1;
	}

	if (FAILED(hr = cBDAChannelScan->createConnectionPoint()))
	{
		cout << "Failed to add connection point to TIF." << endl;
		return 1;
	}

	if (cBDAChannelScan->StartGraph() == FALSE)
	{
		cout << "Failed to start the graph" << endl;
		return 1;
	}

	bExit = FALSE;
	while (bExit == FALSE)
	{
		cout << endl;
		cout << "1. Add Network" << endl;
		cout << "2. Generate Channels.ini" << endl;
		cout << "5. Scan all Australian frequencies"  << endl;
		cout << "6. Turn Verbose output " << (cBDAChannelScan->IsVerbose() ? "Off" : "On") << endl;
		cout << "10.Exit"		 << endl;
		cout << endl;
		cout << "Please Select Menu(1,2,5,6,10):";

		cin >> iMenuSelect;

		switch (iMenuSelect)
		{
		case 1:
			long freq, band;
			GetChannelInfo(freq, band);
			cBDAChannelScan->AddNetwork(freq, band);
			break;

		case 2:
			cBDAChannelScan->scanNetworks();
			break;

		case 5:
			cBDAChannelScan->scanAll();
			break;

		case 6:
			cBDAChannelScan->ToggleVerbose();
			break;

		case 10:
			bExit = TRUE;
			break;

		default:
			cout << "Error selecting,Please Select again" << endl;  
			continue;
		}
	}


	cBDAChannelScan->StopGraph();

	cBDAChannelScan->Release();

	return S_OK;
}

HRESULT GetChannelInfo(long &freq, long &band)
{
	freq = 0;
	band = 0;
	cout << "Frequency (KHz):";
	cin >> freq;
	while (freq <= 0)
	{
		cout << "invalid number. Please try again:";
		cin >> freq;
	}

	cout << "BandWidth (Mbps):";
	cin >> band;
	while (band <= 0)
	{
		cout << "invalid number. Please try again:";
		cin >> band;
	}

	return S_OK;
}

