/**
 *	BDACardCollection.cpp
 *	Copyright (C) 2004 Nate
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

//this is a file from DigitalWatch 2 that i've hacked up to work here.

#include "BDACardCollection.h"
#include "StdAfx.h"
#include "ParseLine.h"

#include "GlobalFunctions.h"
#include <dshow.h>
#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include <bdatypes.h> // Must be included before bdamedia.h
#include <bdamedia.h>
#include "XMLDocument.h"

BDACardCollection::BDACardCollection()
{
	m_filename = NULL;
}

BDACardCollection::~BDACardCollection()
{
	std::vector<BDACard *>::iterator it = cards.begin();
	for ( ; it != cards.end() ; it++ )
	{
		delete *it;
	}
	cards.clear();
}

void BDACardCollection::SetLogCallback(LogMessageCallback *callback)
{
	LogMessageCaller::SetLogCallback(callback);

	graphTools.SetLogCallback(callback);

	std::vector<BDACard *>::iterator it = cards.begin();
	for ( ; it != cards.end() ; it++ )
	{
		BDACard *card = *it;
		card->SetLogCallback(callback);
	}
}

LogMessage BDACardCollection::get_Logger()
{
	return log;
}

BOOL BDACardCollection::LoadCards()
{
	if (m_filename)
		delete m_filename;
	m_filename = NULL;

	return LoadCardsFromHardware();
}

BOOL BDACardCollection::LoadCards(LPWSTR filename)
{
	LoadCardsFromFile(filename);
	return LoadCardsFromHardware();
}

BOOL BDACardCollection::LoadCardsFromFile(LPWSTR filename)
{
	(log << "Loading BDA DVB-T Cards file: " << filename << "\n").Write();
	LogMessageIndent indent(&log);

	strCopy(m_filename, filename);

	XMLDocument file;
	file.SetLogCallback(m_pLogCallback);
	if (file.Load(m_filename) != S_OK)
	{
		return (log << "Could not load cards file: " << m_filename << "\n").Write(false);
	}

	BDACard* currCard = NULL;

	int elementCount = file.Elements.Count();
	for ( int item=0 ; item<elementCount ; item++ )
	{
		XMLElement *element = file.Elements.Item(item);
		if (_wcsicmp(element->name, L"card") != 0)
			continue;

		currCard = new BDACard();
		currCard->SetLogCallback(m_pLogCallback);

		if (currCard->LoadFromXML(element) == S_OK)
			AddCardToList(currCard);
		else
			delete currCard;
	}

	indent.Release();
	(log << "Finished Loading BDA DVB-T Cards file\n").Write();

	return (cards.size() > 0);
}

void BDACardCollection::AddCardToList(BDACard* currCard)
{
	//check if currCard is present and check that all it's values got set before pushing it into the cards vector
	if (currCard)
	{
		if ((currCard->tunerDevice.strDevicePath == NULL) ||
			(currCard->tunerDevice.strFriendlyName == NULL))
		{
			(log << "Error in " << m_filename << ": Cannot add device without setting tuner name and device path\n").Write();
			return;
		}
		cards.push_back(currCard);
	}
}

BOOL BDACardCollection::SaveCards(LPWSTR filename)
{
	XMLDocument file;
	file.SetLogCallback(m_pLogCallback);

	std::vector<BDACard *>::iterator it = cards.begin();
	for ( ; it < cards.end() ; it++ )
	{
		XMLElement *pElement = new XMLElement(L"Card");
		BDACard *pCard = *it;
		pCard->SaveToXML(pElement);
		file.Elements.Add(pElement);
	}
	
	if (filename)
		file.Save(filename);
	else
		file.Save(m_filename);
		
	return TRUE;
}

BOOL BDACardCollection::LoadCardsFromHardware()
{
	HRESULT hr = S_OK;

	(log << "Checking for new BDA DVB-T Cards\n").Write();
	LogMessageIndent indent(&log);

	DirectShowSystemDevice* pTunerDevice;
	DirectShowSystemDeviceEnumerator enumerator(KSCATEGORY_BDA_NETWORK_TUNER);
	enumerator.SetLogCallback(m_pLogCallback);

	while (enumerator.Next(&pTunerDevice) == S_OK)
	{
		(log << "Tuner - " << pTunerDevice->strFriendlyName << "\n").Write();
		LogMessageIndent indentB(&log);
		(log << pTunerDevice->strDevicePath << "\n").Write();
		LogMessageIndent indentC(&log);

		BDACard *bdaCard = NULL;

		std::vector<BDACard *>::iterator it = cards.begin();
		for ( ; it != cards.end() ; it++ )
		{
			bdaCard = *it;
			if (_wcsicmp(pTunerDevice->strDevicePath, bdaCard->tunerDevice.strDevicePath) == 0)
				break;
			bdaCard = NULL;
		}

		if (bdaCard)
		{
			(log << "This tuner was loaded from file\n").Write();
			bdaCard->bDetected = TRUE;
			//TODO: maybe?? verify tuner can connect to the capture filter.
		}
		else
		{
			DirectShowSystemDevice* pDemodDevice;
			DirectShowSystemDevice* pCaptureDevice;

			if (FindCaptureDevice(pTunerDevice, &pDemodDevice, &pCaptureDevice))
			{
				bdaCard = new BDACard();
				bdaCard->SetLogCallback(m_pLogCallback);

				bdaCard->bActive = TRUE;
				bdaCard->bNew = TRUE;
				bdaCard->bDetected = TRUE;

				bdaCard->tunerDevice = *pTunerDevice;
				if (pDemodDevice)
				{
					(log << "Demod - " << pDemodDevice->strFriendlyName << "\n").Write();
					LogMessageIndent indentD(&log);
					(log << pDemodDevice->strDevicePath << "\n").Write();
					bdaCard->demodDevice = *pDemodDevice;
				}
				if (pCaptureDevice)
				{
					(log << "Capture - " << pCaptureDevice->strFriendlyName << "\n").Write();
					LogMessageIndent indentD(&log);
					(log << pCaptureDevice->strDevicePath << "\n").Write();
					bdaCard->captureDevice = *pCaptureDevice;
				}

				cards.push_back(bdaCard);
			}
		}

		delete pTunerDevice;
		pTunerDevice = NULL;
	}

	if (cards.size() == 0)
		return (log << "No cards found\n").Show(FALSE);

	indent.Release();
	(log << "Finished Checking for new BDA DVB-T Cards\n").Write();

	return TRUE;
}

BOOL BDACardCollection::FindCaptureDevice(DirectShowSystemDevice* pTunerDevice, DirectShowSystemDevice** ppDemodDevice, DirectShowSystemDevice** ppCaptureDevice)
{
	HRESULT hr;
	CComPtr <IGraphBuilder> piGraphBuilder;
	CComPtr <IBaseFilter> piBDANetworkProvider;
	CComPtr <ITuningSpace> piTuningSpace;
	CComPtr <ITuneRequest> pTuneRequest;
	CComPtr <IBaseFilter> piBDATuner;
	CComPtr <IBaseFilter> piBDADemod;
	CComPtr <IBaseFilter> piBDACapture;

	BOOL bRemoveNP = FALSE;
	BOOL bRemoveTuner = FALSE;
	BOOL bFoundDemod = FALSE;
	BOOL bFoundCapture = FALSE;

	(log << "Finding Demod and Capture filters\n").Write();

	do
	{
		//--- Create Graph ---
		if FAILED(hr = piGraphBuilder.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER))
		{
			//(log << "Failed Creating DVB-T Graph Builder\n").Write();
			break;
		}

		//--- Initialise Tune Request ---
		if FAILED(hr = graphTools.InitDVBTTuningSpace(piTuningSpace))
		{
			//(log << "Failed to initialise the Tune Request\n").Write();
			break;
		}

		//--- Get NetworkType CLSID ---
		CComBSTR bstrNetworkType;
		CLSID CLSIDNetworkType;
		if FAILED(hr = piTuningSpace->get_NetworkType(&bstrNetworkType))
		{
			//(log << "Failed to get TuningSpace Network Type\n").Write();
			break;
		}

		if FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType))
		{
			//(log << "Could not convert Network Type to CLSID\n").Write();
			break;
		}

		//--- Create Network Provider ---
		if FAILED(hr = graphTools.AddFilter(piGraphBuilder, CLSIDNetworkType, &piBDANetworkProvider, L"Network Provider"))
		{
			//(log << "Failed to add Network Provider to the graph\n").Write();
			break;
		}
		bRemoveNP = TRUE;

		//--- Create TuneRequest ---
		if FAILED(hr = graphTools.CreateDVBTTuneRequest(piTuningSpace, pTuneRequest, -1, -1))
		{
			//(log << "Failed to create the Tune Request.\n").Write();
			break;
		}

		//--- Apply TuneRequest ---
		CComQIPtr <ITuner> pTuner(piBDANetworkProvider);
		if (!pTuner)
		{
			//(log << "Failed while interfacing Tuner with Network Provider\n").Write();
			break;
		}

		if FAILED(hr = pTuner->put_TuneRequest(pTuneRequest))
		{
			//(log << "Failed to submit the Tune Tequest to the Network Provider\n").Write();
			break;
		}

		//--- We can now add and connect the tuner filter ---
		
		if FAILED(hr = graphTools.AddFilterByDevicePath(piGraphBuilder, &piBDATuner, pTunerDevice->strDevicePath, pTunerDevice->strFriendlyName))
		{
			//(log << "Cannot load Tuner Device\n").Write();
			break;
		}
		bRemoveTuner = TRUE;
    
		if FAILED(hr = graphTools.ConnectFilters(piGraphBuilder, piBDANetworkProvider, piBDATuner))
		{
			//(log << "Failed to connect Network Provider to Tuner Filter\n").Write();
			break;
		}

		LogMessageIndent indent(&log);

		DirectShowSystemDeviceEnumerator enumerator(KSCATEGORY_BDA_RECEIVER_COMPONENT);
		*ppDemodDevice = NULL;
		while (hr = enumerator.Next(ppDemodDevice) == S_OK)
		{
			(log << "Trying - " << (*ppDemodDevice)->strFriendlyName << "\n").Write();
			LogMessageIndent indentB(&log);
			(log << (*ppDemodDevice)->strDevicePath << "\n").Write();
			LogMessageIndent indentC(&log);

			if FAILED(hr = graphTools.AddFilterByDevicePath(piGraphBuilder, &piBDADemod, (*ppDemodDevice)->strDevicePath, (*ppDemodDevice)->strFriendlyName))
			{
				break;
			}

			if SUCCEEDED(hr = graphTools.ConnectFilters(piGraphBuilder, piBDATuner, piBDADemod))
			{
				(log << "SUCCESS\n").Write();
				bFoundDemod = TRUE;
				break;
			}

			//(log << "Connection failed, trying next card\n").Write();
			piBDADemod.Release();
			delete *ppDemodDevice;
			*ppDemodDevice = NULL;
		}

		if (bFoundDemod)
		{
			(log << "Checking for Capture filter\n").Write();
			LogMessageIndent indentB(&log);
			*ppCaptureDevice = NULL;
			enumerator.Reset();
			while (hr = enumerator.Next(ppCaptureDevice) == S_OK)
			{
				//Don't try to connect the one we already found
				if (_wcsicmp((*ppDemodDevice)->strDevicePath, (*ppCaptureDevice)->strDevicePath) == 0)
				{
					delete *ppCaptureDevice;
					*ppCaptureDevice = NULL;
					continue;
				}

				(log << "Trying - " << (*ppCaptureDevice)->strFriendlyName << "\n").Write();
				LogMessageIndent indentC(&log);
				(log << (*ppCaptureDevice)->strDevicePath << "\n").Write();
				LogMessageIndent indentD(&log);

				if FAILED(hr = graphTools.AddFilterByDevicePath(piGraphBuilder, &piBDACapture, (*ppCaptureDevice)->strDevicePath, (*ppCaptureDevice)->strFriendlyName))
				{
					break;
				}

				if SUCCEEDED(hr = graphTools.ConnectFilters(piGraphBuilder, piBDADemod, piBDACapture))
				{
					(log << "SUCCESS\n").Write();
					bFoundCapture = TRUE;
					break;
				}

				//(log << "Connection failed, trying next card\n").Write();
				piBDACapture.Release();
				delete *ppCaptureDevice;
				*ppCaptureDevice = NULL;
			}
		}

		//if (hr != S_OK)
			//(log << "No Cards Left\n").Write();
	} while (FALSE);

	graphTools.DisconnectAllPins(piGraphBuilder);
	if (bFoundCapture)
		piGraphBuilder->RemoveFilter(piBDACapture);
	if (bFoundDemod)
		piGraphBuilder->RemoveFilter(piBDADemod);
	if (bRemoveTuner)
		piGraphBuilder->RemoveFilter(piBDATuner);
	if (bRemoveNP)
		piGraphBuilder->RemoveFilter(piBDANetworkProvider);

	piBDACapture.Release();
	piBDADemod.Release();
	piBDATuner.Release();
	piBDANetworkProvider.Release();

	pTuneRequest.Release();
	piTuningSpace.Release();
	piGraphBuilder.Release();

	if (!bFoundCapture)
	{
		*ppCaptureDevice = *ppDemodDevice;
		*ppDemodDevice = NULL;
	}

	(log << "Finished Finding Demod and Capture filters\n").Write();

	return (bFoundDemod);
}

