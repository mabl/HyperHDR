/* DiscoveryBrowser.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */


#include <bonjour/DiscoveryWrapper.h>
#include <leddevice/LedDevice.h>
#include <leddevice/LedDeviceFactory.h>

#include <QTimer>
#include <QString>
#include <QStringLiteral>
#include <QNetworkInterface>
#include <QMutableListIterator>
#include <cstdio>
#include <utils/Logger.h>

DiscoveryWrapper* DiscoveryWrapper::instance = nullptr;

DiscoveryWrapper::DiscoveryWrapper(QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance(QString("NET_DISCOVERY")))
	, _serialDevice(nullptr)
{
	// register meta
	qRegisterMetaType<DiscoveryRecord>("DiscoveryRecord");
	qRegisterMetaType<QList<DiscoveryRecord>>("QList<DiscoveryRecord>");
	qRegisterMetaType<DiscoveryRecord::Service>("DiscoveryRecord::Service");

	DiscoveryWrapper::instance = this;

	connect(this, &DiscoveryWrapper::discoveryEvent, this, &DiscoveryWrapper::discoveryEventHandler);
	connect(this, &DiscoveryWrapper::requestToScan, this, &DiscoveryWrapper::requestToScanHandler);	
}

DiscoveryWrapper::~DiscoveryWrapper()
{
	if (_serialDevice != nullptr)
		delete _serialDevice;
	_serialDevice = nullptr;
}

void DiscoveryWrapper::cleanUp(QList<DiscoveryRecord>& target)
{
	QMutableListIterator<DiscoveryRecord> i(target);
	DiscoveryRecord::Service action = DiscoveryRecord::Service::Unknown;

	while (i.hasNext())
	{
		if (i.next().expired())
		{
			DiscoveryRecord& message = i.value();
			QString log = QString("%1 %2 at %3:%4 (%5)").arg("Removing not responding").arg(message.getName()).arg(message.address).arg(message.port).arg(message.hostName);

			Warning(_log, "%s", QSTRING_CSTR(log));
			action = message.type;
			i.remove();			
		}
	}

	if (action != DiscoveryRecord::Service::Unknown)
		emit foundService(action, target);
}

QList<DiscoveryRecord> DiscoveryWrapper::getPhilipsHUE()
{
	cleanUp(_hueDevices);

	emit requestToScan(DiscoveryRecord::Service::PhilipsHue);

	return _hueDevices;
}

QList<DiscoveryRecord> DiscoveryWrapper::getWLED()
{
	cleanUp(_wledDevices);

	emit requestToScan(DiscoveryRecord::Service::WLED);

	return _wledDevices;
}

QList<DiscoveryRecord> DiscoveryWrapper::getHyperHDRServices()
{
	return _hyperhdrSessions;
}

QList<DiscoveryRecord> DiscoveryWrapper::getAllServices()
{
	return _hyperhdrSessions + _esp32s2Devices + _espDevices + _hueDevices + _picoDevices + _wledDevices;
}

void DiscoveryWrapper::requestServicesScan()
{
	cleanUp(_wledDevices);
	emit requestToScan(DiscoveryRecord::Service::WLED);
	cleanUp(_hueDevices);
	emit requestToScan(DiscoveryRecord::Service::PhilipsHue);
	cleanUp(_hyperhdrSessions);
	emit requestToScan(DiscoveryRecord::Service::HyperHDR);

	cleanUp(_esp32s2Devices);
	cleanUp(_espDevices);
	cleanUp(_picoDevices);
	emit requestToScan(DiscoveryRecord::Service::SerialPort);
}

void DiscoveryWrapper::gotMessage(QList<DiscoveryRecord>& target, DiscoveryRecord message)
{
	QList<DiscoveryRecord> newSessions;

	const QHostAddress& testHost = QHostAddress(message.address);
	for (const QHostAddress& address : QNetworkInterface::allAddresses())
		if (address == testHost)
		{
			return;
		}

	if (message.isExists)
	{
		for (DiscoveryRecord& rec : target)
			if (rec == message)
			{
				rec.resetTTL();
				return;
			}

		newSessions = target;
		newSessions.append(message);
	}
	else
	{
		for (const DiscoveryRecord& rec : target)
			if (rec != message)
			{
				newSessions.append(rec);
			}
	}

	if (target.length() != newSessions.length())
	{
		QString log = QString("%1 %2 at %3:%4 (%5)").arg((message.isExists) ? "Found" : "Deregistered").arg(message.getName()).arg(message.address).arg(message.port).arg(message.hostName);

		Info(_log, "%s", QSTRING_CSTR(log));
		target = newSessions;
		emit foundService(message.type, target);
	}
}

void DiscoveryWrapper::discoveryEventHandler(DiscoveryRecord message)
{
	if (message.type == DiscoveryRecord::Service::HyperHDR)
		gotMessage(_hyperhdrSessions, message);
	else if (message.type == DiscoveryRecord::Service::WLED)
		gotMessage(_wledDevices, message);
	else if (message.type == DiscoveryRecord::Service::PhilipsHue)
		gotMessage(_hueDevices, message);
	else if (message.type == DiscoveryRecord::Service::Pico)
		gotMessage(_picoDevices, message);
	else if (message.type == DiscoveryRecord::Service::ESP32_S2)
		gotMessage(_esp32s2Devices, message);
	else if (message.type == DiscoveryRecord::Service::ESP)
		gotMessage(_espDevices, message);
}

void DiscoveryWrapper::requestToScanHandler(DiscoveryRecord::Service type)
{
	if (type == DiscoveryRecord::Service::SerialPort)
	{
		if (_serialDevice == nullptr)
		{
			QJsonObject deviceConfig;
			deviceConfig["type"] = "adalight";

			_serialDevice = LedDeviceFactory::construct(deviceConfig);
		}
		QJsonObject params;
		QJsonObject devicesDiscovered = _serialDevice->discover(params);		
	}
}
