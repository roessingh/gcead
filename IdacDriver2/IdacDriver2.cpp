/**
 * Copyright (C) 2009  Ellis Whitehead
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "IdacDriver2.h"

#include <iostream>

#include <usb.h>

#include <QtDebug>
#include <QFile>
#include <QMutex>
#include <QThread>

#include <Check.h>

#include <IdacDriver/IdacDriverSamplingThread.h>
#include <IdacDriver/Sleeper.h>

#include "IdacDriver2Constants.h"
#include "IdacDriver2ReqIds.h"

using namespace std;


extern INTEL_HEX_RECORD g_firmwareIdacDriver2[];

/// Voltage in uVolt
#define MAX_INPUT_VOLTAGE_ADC 1000000

static int dwRangesList[IDAC_SCALERANGECOUNT+1] =
{
	MAX_INPUT_VOLTAGE_ADC / 1,
	MAX_INPUT_VOLTAGE_ADC / 4,
	MAX_INPUT_VOLTAGE_ADC / 16,
	MAX_INPUT_VOLTAGE_ADC / 64,
	MAX_INPUT_VOLTAGE_ADC / 256,
	MAX_INPUT_VOLTAGE_ADC / 1024,
	-1
};

// Sampling variables
static IdacDriverSamplingThread* g_recordThread;
static int g_iDecimation;
static int g_nDigitalSum;
static int g_nAnalog1Sum;
static int g_nAnalog2Sum;
static int g_nSamplesInBuffer;
const int g_nSampleMax = 600;
static short g_samplesDigital[g_nSampleMax];
static short g_samplesAnalog1[g_nSampleMax];
static short g_samplesAnalog2[g_nSampleMax];
static int g_iSampleRead;
static int g_iSampleWrite;
static QMutex g_sampleMutex;


IdacDriver2::IdacDriver2(struct usb_device* device, QObject* parent)
	: IdacDriverUsb(device, parent)
{
	m_bFpgaProgrammed = false;

	m_bSampling = false;

	setHardwareName("IDAC2");
}

IdacDriver2::~IdacDriver2()
{
	if (handle() != NULL)
	{
		setIsoXferEnabled(false);
		setPowerOn(false);
	}
}

bool IdacDriver2::checkUsbFirmwareReady()
{
	CHECK_PRECOND_RETVAL(device() != NULL, false);

	bool b = false;
	if (device()->config[0].bNumInterfaces == 1)
		if (device()->config[0].interface[0].num_altsetting == 1)
			b = true;

	return b;
}

bool IdacDriver2::checkDataFirmwareReady()
{
	return m_bFpgaProgrammed;
}

void IdacDriver2::initUsbFirmware()
{
	CHECK_PRECOND_RET(handle() != NULL);

	int res;

	struct usb_device* dev = device();
	int idConfiguration = dev->config[0].bConfigurationValue;
	res = usb_set_configuration(handle(), idConfiguration);
	if (res < 0)
		return;

	int idInterface = dev->config[0].interface[0].altsetting[0].bInterfaceNumber;
	res = usb_claim_interface(handle(), idInterface);
	if (res < 0)
		return;

	usb_interface_descriptor* setting = &dev->config[0].interface[0].altsetting[0];
	res = usb_set_altinterface(handle(), setting->bAlternateSetting);
	if (res < 0)
		return;

	sendFirmware(g_firmwareIdacDriver2);
}

void IdacDriver2::initDataFirmware()
{
	QString sFilename = "idc2fpga.hex";
	quint8 buffer[16];

	if (!claim())
		return;

	setPowerOn(false);
	setPowerOn(true);

	// Program FPGA
	for (int nRetries = 6; nRetries > 0; nRetries--)
	{
		// Program FPGA
		sendOutgoingMessage(REQUESTID_PROGRAM_FPGA_START);
		bool bResult = sendBinFile(sFilename);
		sendOutgoingMessage(REQUESTID_PROGRAM_FPGA_STOP);

		if (!bResult)
			break;				// Problem sending FPGA file

		// fetch status?
		sendIncomingMessage(0x22, buffer, 1);
		// TODO: I don't know what "7" means -- ellis, 2009-04-26
		if (buffer[0] == 0x07) {
			m_bFpgaProgrammed = true;
			break;
		}
	}

	// if, after 6 retries, the FPGA still isn't booted
	if (!m_bFpgaProgrammed)
	{
		return;
	}

	// Reset FPGA?
	sendOutgoingMessage(0x20);

	// Fetch config?
	// 1738193668 S Ci:3:005:0 s c0 2c 0000 0000 0010 16 <
	// 1738204153 C Ci:3:005:0 0 16 = 005a3c00 99ffffff ffffffff ffffffff
	sendIncomingMessage(0x2C, buffer, 16);

	initStringsAndRanges();

	// Init channels?
	/*
	1738207870 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 808080
	1738217771 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	1738221902 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 808080
	1738229919 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	1738237848 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 808080
	1738245649 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	1738249464 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 ffe6a0
	1738250977 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	1738255632 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 ffe6a0
	1738257047 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	1738261250 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 00808f80 ffe6a0
	1738269804 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	*/
	/*
	buffer[0] = 0x00;
	buffer[1] = 0x80;
	buffer[2] = 0x8f;
	buffer[3] = 0x80;
	buffer[4] = 0x80;
	buffer[5] = 0x80;
	buffer[6] = 0x80;
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, buffer, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, buffer, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, buffer, 1);
	buffer[0] = 0x40;
	buffer[3] = 0x90;
	buffer[4] = 0xff;
	buffer[5] = 0xe6;
	buffer[6] = 0xb0;
	quint8 zero[1] = { 0x00 };
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	*/
	sendChannelSettings();
}

void IdacDriver2::initStringsAndRanges()
{
	setLowpassStrings(QStringList());
	setHighpassStrings(QStringList()
			<< tr("DC")
			<< tr("0.05Hz")
			<< tr("0.1Hz")
			<< tr("0.2Hz")
			<< tr("0.5Hz")
			<< tr("1Hz")
			<< tr("2Hz")
			<< tr("5Hz"));

	QList<int> list;
	int* ranges = dwRangesList;
	while (*ranges != -1)
		list << *ranges++;
	setRanges(list);
}

bool IdacDriver2::claim()
{
	int res;

	struct usb_device* dev = device();
	int idConfiguration = dev->config[0].bConfigurationValue;
	res = usb_set_configuration(handle(), idConfiguration);
	if (res < 0)
		return false;

	int idInterface = dev->config[0].interface[0].altsetting[0].bInterfaceNumber;
	res = usb_claim_interface(handle(), idInterface);
	if (res < 0)
		return false;

	usb_interface_descriptor* setting = &dev->config[0].interface[0].altsetting[0];
	res = usb_set_altinterface(handle(), setting->bAlternateSetting);
	if (res < 0)
		return false;

	for (int iPipe = 0; iPipe < setting->bNumEndpoints; iPipe++)
	{
		int idPipe = setting->endpoint[iPipe].bEndpointAddress;
		res = usb_clear_halt(handle(), idPipe);
		if (res < 0)
			return false;
	}

	return true;
}

void IdacDriver2::loadDefaultChannelSettings(IdacChannelSettings* channels)
{
	channels[0].mEnabled = 0x03;
	channels[0].mInvert = 0x00;
	channels[0].nDecimation = -1;

	channels[1].mEnabled = 1;
	channels[1].mInvert = 0;
	channels[1].nDecimation = -1;
	channels[1].iRange = 3;
	channels[1].iLowpass = -1;
	channels[1].iHighpass = 1; // 0.05 Hz on IDAC2
	channels[1].nExternalAmplification = 10;

	channels[2].mEnabled = 1;
	channels[2].mInvert = 0;
	channels[2].nDecimation = -1;
	channels[2].iRange = 4;
	channels[2].iLowpass = -1;
	channels[2].iHighpass = 1; // 0.05 Hz on IDAC2
	channels[2].nExternalAmplification = 1;
}

void IdacDriver2::configureChannel(int iChan)
{
	const IdacChannelSettings* chan = desiredChannelSettings(iChan);
	CHECK_ASSERT_RET(chan != NULL);

	*actualChannelSettings(iChan) = *chan;
	sendChannelSettings();
}

bool IdacDriver2::setPowerOn(bool bOn)
{
	return sendOutgoingMessage((bOn) ? REQUESTID_POWER_ON : REQUESTID_POWER_OFF);
}

bool IdacDriver2::sendChannelSettings()
{
	IdacChannelSettings* chans = actualSettings();

	quint8 buffer[7];

	buffer[0] = chans[0].iRange << 4;

	int n, n1, n2, n3, code;

	n = chans[1].nOffset;
	n1 = n & 0x03;
	n2 = (n >> 2) & 0x7f;
	n3 = (n >> 9) & 0x7f;
	code = 0x00808080 | (n1 << 5) | (n2 << 8) | (n3 << 16);
	if (chans[1].iLowpass > 0)
		code |= 0x10 | (chans[1].iLowpass << 1);
	buffer[1] = (code >> 16) & 0xff;
	buffer[2] = (code >>  8) & 0xff;
	buffer[3] = (code >>  0) & 0xff;

	n = chans[2].nOffset;
	n1 = n & 0x03;
	n2 = (n >> 2) & 0x7f;
	n3 = (n >> 9) & 0x7f;
	code = 0x00808080 | (n1 << 5) | (n2 << 8) | (n3 << 16);
	if (chans[2].iHighpass > 0)
		code |= 0x10 | (chans[2].iHighpass << 1);
	buffer[4] = (code >> 16) & 0xff;
	buffer[5] = (code >>  8) & 0xff;
	buffer[6] = (code >>  0) & 0xff;
	//buffer[3] |= 00=DC, 10=0.05Hz, 12=0.1Hz, 14=0.2Hz, 16=0.5Hz, 18=1Hz, 1A=2Hz, 1C=5Hz
	//buffer[6] |= 00=DC, 10=0.05Hz, 12=0.1Hz, 14=0.2Hz, 16=0.5Hz, 18=1Hz, 1A=2Hz, 1C=5Hz

	sendOutgoingMessage(REQUESTID_WRITE_CHANNEL_SETTINGS, buffer, 7);

	// 406739825 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	quint8 zero[1] = { 0 };
	sendOutgoingMessage(0x23, zero, 1);

	return true;
}

// Turn sample generation on/off
bool IdacDriver2::setSamplingPaused(bool bPause)
{
	if (m_bSampling)
	{
		if (bPause)
			m_bSamplingPaused = true;

		setIsoXferEnabled(!bPause);

		// Allow for isochrone latency (restart may be unsafe)
		if (bPause)
			Sleeper::msleep(IDAC_BUFFER_DELAY);
	}

	return true;
}

bool IdacDriver2::setIsoXferEnabled(bool bEnabled)
{
	int requestId = (bEnabled) ? REQUESTID_ISO_IN_START : REQUESTID_ISO_IN_STOP; // FIXME: I don't know if these are the proper codes, and if so, this function should be called from sampleLoop() -- ellis, 2009-04-26
	bool b = sendOutgoingMessage(requestId);
	if (b && bEnabled)
		m_bSamplingPaused = false;
	return b;
}

bool IdacDriver2::startSampling()
{
	quint8 buffer[8];

	// Get status?
	// 406727620 S Ci:3:005:0 s c0 22 0000 0000 0001 1 <
	// 406728490 C Ci:3:005:0 0 1 = 07
	sendIncomingMessage(0x22, buffer, 1);

	/*
	// 406733274 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 10808f80 ffe6a0
	// 406739825 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	// 406747751 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 10808f80 ffe6a0
	// 406751745 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	// 406756493 S Co:3:005:0 s 40 28 0000 0000 0007 7 = 10808f80 ffe6a0
	// 406759713 S Co:3:005:0 s 40 23 0000 0000 0001 1 = 00
	buffer[0] = 0x40;
	buffer[1] = 0x80;
	buffer[2] = 0x8f;
	buffer[3] = 0x90;
	buffer[4] = 0xff;
	buffer[5] = 0xe6;
	buffer[6] = 0xb0;
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	sendOutgoingMessage(0x28, buffer, 7);
	sendOutgoingMessage(0x23, zero, 1);
	*/
	for (int iChan = 0; iChan < 3; iChan++)
		*actualChannelSettings(iChan) = *desiredChannelSettings(iChan);
	sendChannelSettings();

	sampleStart();
	return true;
}

void IdacDriver2::sampleStart()
{
	CHECK_PRECOND_RET(g_recordThread == NULL);

	m_bSampling = true;
	g_iSampleRead = 0;
	g_iSampleWrite = 0;
	g_nSamplesInBuffer = 0;
	g_recordThread = new IdacDriverSamplingThread(this);
	g_recordThread->start(QThread::TimeCriticalPriority);
}

void IdacDriver2::sampleLoop()
{
	int ret;

	// Begin ISO xfer?
	// 406772916 S Co:3:005:0 s 40 29 0000 0000 0000 0
	setIsoXferEnabled(true);

	// 406779815 S Co:3:005:0 s 02 01 0000 0081 0000 0
	usb_control_msg(handle(), 0x02, 0x01, 0, 0x0081, NULL, 0, 0);

	g_iDecimation = 0;
	g_nDigitalSum = 0;
	g_nAnalog1Sum = 0;
	g_nAnalog2Sum = 0;

	// Now loop till done
	quint8 buffer[51];
	bool bSamplingPrev = m_bSampling;
	bool bOverflow = false;
	int iPart = 0;
	quint8 parts[8];
	parts[7] = 0;
	while (bSamplingPrev)
	{
		bool bSamplingNow = m_bSampling;

		ret = usb_interrupt_read(handle(), 0x81, (char*) buffer, 51, 5000);
		if (ret < 0)
		{
			cout << "INTERRUPT READ ret = " << ret << " " << usb_strerror() << endl;
		}

		//int nPackets = (ret > 0) ? (ret / 51) : 0;
		if (ret > 0)
		{
			quint8* p = (quint8*) buffer;
			int nBytes = *p++;
			//cout << "[" << nBytes << "] ";
			for (int iByte = 0; iByte < nBytes; iByte++) {
				/*
				QString sHex = QString::number(p[iByte], 16);
				if (sHex.size() == 1)
					sHex = "0" + sHex;
				cout << qPrintable(sHex);
				*/

				parts[iPart++] = p[iByte];
				if (iPart == 5)
				{
					CHECK_ASSERT_NORET((parts[0] & 0x80) == 0);
					CHECK_ASSERT_NORET((parts[1] & 0x80) != 0);
					CHECK_ASSERT_NORET((parts[2] & 0x80) != 0);
					CHECK_ASSERT_NORET((parts[3] & 0x80) != 0);
					CHECK_ASSERT_NORET((parts[4] & 0x80) != 0);

					for (int i = 0; i < 5; i++)
						parts[i] &= 0x7f;

					short analog1 = (parts[0] << 9) | (parts[1] << 2) | (parts[2] >> 5);
					short analog2 = ((parts[2] & 0x1f) << 11) | (parts[3] << 4) | (parts[4] >> 3);
					short digital = 0;
					digital |= ((parts[4] & 0x02) > 0);
					digital |= ((parts[4] & 0x01) > 0) << 1;

					/*
					cout << "\t" << analog1;
					cout << "\t" << analog2;
					cout << "\t" << ((parts[4] & 0x02) > 0 ? 'X' : '_');
					cout << "\t" << ((parts[4] & 0x01) > 0 ? 'X' : '_');
					cout << endl;
					*/

					iPart = 0;

					g_nAnalog1Sum += analog1;
					g_nAnalog2Sum += analog2;
					g_nDigitalSum = digital;
					g_iDecimation++;

					if (g_iDecimation == 5)
					{
						analog1 = g_nAnalog1Sum / 5;
						analog2 = g_nAnalog2Sum / 5;

						g_iDecimation = 0;
						g_nAnalog1Sum = 0;
						g_nAnalog2Sum = 0;

						// A sample was produced
						if (g_nSamplesInBuffer < g_nSampleMax - 1)
						{
							g_samplesDigital[g_iSampleWrite] = digital;
							g_samplesAnalog1[g_iSampleWrite] = analog1;
							g_samplesAnalog2[g_iSampleWrite] = analog2;

							g_iSampleWrite++;
							if (g_iSampleWrite == g_nSampleMax)
								g_iSampleWrite = 0;
							g_sampleMutex.lock();
							g_nSamplesInBuffer++;
							g_sampleMutex.unlock();
						}
						else
						{
							bOverflow = true;
							//m_bSampling = false;
							printf("OVERFLOW\n");
						}
					}
				}
			}
		}
		bSamplingPrev = bSamplingNow;
	}

	// End of ISO xfer?
	// 412307451 S Co:3:005:0 s 40 2a 0000 0000 0000 0
	setIsoXferEnabled(false);
}

void IdacDriver2::stopSampling()
{
	CHECK_PRECOND_RET(g_recordThread != NULL);

	m_bSampling = false;
	if (g_recordThread->wait(5000))
	{
		delete g_recordThread;
		g_recordThread = NULL;
	}
}

int IdacDriver2::takeData(short* digital, short* analog1, short* analog2, int maxSize)
{
	int size = 0;
	while (size < maxSize && g_nSamplesInBuffer > 0)
	{
		size++;
		*digital++ = g_samplesDigital[g_iSampleRead];
		*analog1++ = g_samplesAnalog1[g_iSampleRead];
		*analog2++ = g_samplesAnalog2[g_iSampleRead];
		g_iSampleRead++;
		if (g_iSampleRead == g_nSampleMax)
			g_iSampleRead = 0;

		g_sampleMutex.lock();
		g_nSamplesInBuffer--;
		g_sampleMutex.unlock();
	}
	return size;
}
