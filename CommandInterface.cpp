//
// Created by ebassetti on 23/07/15.
//

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include "common.h"
#include "CommandInterface.h"
#include "Log.h"
#include "Utils.h"
#include "net/NTP.h"
#include "net/NetworkManager.h"
#include "net/HTTPClient.h"
#include "generic.h"
#include "LED.h"
#include "net/TraceAccumulator.h"

Udp CommandInterface::cmdc;

bool CommandInterface::readPacket(PACKET *pkt) {
	byte pktBuffer[PACKET_SIZE];

	memset(pktBuffer, 0, PACKET_SIZE);
	IPaddr src(0);
	unsigned short port = 0;
	ssize_t cread = cmdc.receive(pktBuffer, (size_t) PACKET_SIZE, &src, &port);
	if (cread > 0) {  // if it received a packet

		if (cread == PACKET_SIZE && memcmp("INGV\0", pktBuffer, 5) == 0) {

			pkt->type = (PacketType) pktBuffer[5];
			pkt->source = src;

			if (pkt->type == PKTTYPE_SENDGPS) {
				memcpy(pkt->mac, pktBuffer + 6, 6);

				float latitude, longitude;

				memcpy(&latitude, pktBuffer + 12, 4);
				memcpy(&longitude, pktBuffer + 16, 4);

				pkt->latitude = Utils::reverseFloat(latitude);
				pkt->longitude = Utils::reverseFloat(longitude);
			} else if (pkt->type == PKTTYPE_SETSYSLOG) {
				uint32_t ip = 0;
				memcpy(&ip, pktBuffer + 6, 4);
				pkt->syslogServer = IPaddr(ip);
			}
			return true;
		}
	}
	return false;
}

void CommandInterface::sendPacket(PACKET pkt) {
	byte pktbuf[PACKET_SIZE];
	memset(pktbuf, 0, PACKET_SIZE);

	memcpy(pktbuf, "INGV\0", 5);
	pktbuf[5] = pkt.type;

	if (pkt.type == PKTTYPE_DISCOVERY_REPLY) {

		byte mac[6];
		Config::getMacAddressAsByte(mac);

		memcpy(pktbuf + 6, mac, 6);

		memcpy(pktbuf + 12, SOFTWARE_VERSION, 4);
		memcpy(pktbuf + 16, PLATFORM_TAG, MINVAL(strlen(PLATFORM_TAG), 8));
	} else if (pkt.type == PKTTYPE_GETINFO_REPLY) {
		int offset = 6;
		memcpy(pktbuf + offset, pkt.mac, 6);
		offset += 6;

		uint32_t ip = pkt.syslogServer;
		memcpy(pktbuf + offset, &ip, 4);
		offset += 4;

		memcpy(pktbuf + offset, &pkt.threshold, 4);
		offset += 4;

		memcpy(pktbuf + offset, &pkt.uptime, 4);
		offset += 4;

		memcpy(pktbuf + offset, &pkt.unixts, 4);
		offset += 4;

		memcpy(pktbuf + offset, pkt.softwareVersion, 4);
		offset += 4;

		memcpy(pktbuf + offset, &pkt.freeRam, 4);
		offset += 4;

		memcpy(pktbuf + offset, &pkt.latency, 4);
		offset += 4;

		ip = pkt.ntpServer;
		memcpy(pktbuf + offset, &ip, 4);
		offset += 4;

		strncpy((char *) (pktbuf + offset), pkt.httpBaseAddress.c_str(), 169);
		offset += MINVAL(pkt.httpBaseAddress.length(), 169) + 1;

		strncpy((char *) (pktbuf + offset), pkt.platformName.c_str(), 19);
		offset += MINVAL(pkt.platformName.length(), 19) + 1;

		strncpy((char *) (pktbuf + offset), pkt.accelerometerName.c_str(), 9);
		offset += MINVAL(pkt.accelerometerName.length(), 9) + 1;

		memcpy(pktbuf + offset, &pkt.statProbeSpeed, 4);
		offset += 4;
	}

	cmdc.send(pktbuf, (size_t) PACKET_SIZE, pkt.source, CMD_INTERFACE_PORT);
}

// check if the mobile APP sent a command to the device
void CommandInterface::checkCommandPacket() {

	PACKET pkt;
	if (!CommandInterface::readPacket(&pkt)) {
		return;
	}

	switch (pkt.type) {
		case PKTTYPE_DISCOVERY: // Discovery
		{
			Log::d("DISCOVERY");
			pkt.type = PKTTYPE_DISCOVERY_REPLY;
			sendPacket(pkt);
		}
			break;
		case PKYTYPE_PING: // Ping
		{
			Log::d("PING");
			pkt.type = PKYTYPE_PONG;
			sendPacket(pkt);
		}
			break;
/*		case PKTTYPE_START: // Start
		{
			Log::d("START");
			udpDest = pkt.source;
			pkt.type = PKTTYPE_OK;
			sendPacket(pkt);
		}
			break;
		case PKTTYPE_STOP: // Stop
		{
			Log::d("STOP");
			udpDest = 0;
			pkt.type = PKTTYPE_OK;
			sendPacket(pkt);
		}
			break;*/
		case PKTTYPE_SENDGPS: // GPS Location
		{
			byte myMac[6];
			Config::getMacAddressAsByte(myMac);
			if (memcmp(myMac, pkt.mac, 6) != 0) return;

			// Reply
			Config::setLongitude(pkt.longitude);
			Config::setLatitude(pkt.latitude);

			Log::d("Location received - latitude: %lf - longitude: %lf", pkt.latitude, pkt.longitude);

			pkt.type = PKTTYPE_OK;
			sendPacket(pkt);
		}
			break;
		case PKTTYPE_SETSYSLOG: {
			Log::setSyslogServer(pkt.syslogServer);

			pkt.type = PKTTYPE_OK;
			sendPacket(pkt);
		}
			break;
		case PKTTYPE_REBOOT: // Request reboot
		{
			pkt.type = PKTTYPE_OK;
			sendPacket(pkt);

			Log::i("Requesting reboot from remote host %s", pkt.source.asString().c_str());
			LED::green(false);
			LED::yellow(false);
			LED::red(true);
			platformReboot();
		}
		case PKTTYPE_GETINFO: {
			// SEND INFO
			Config::getMacAddressAsByte(pkt.mac);
			pkt.syslogServer = Log::getSyslogServer();
			pkt.threshold = (float)Seismometer::getInstance()->getQuakeThreshold();
			pkt.uptime = Utils::uptime();
			pkt.unixts = (uint32_t) NTP::getUNIXTime();
			memcpy(pkt.softwareVersion, SOFTWARE_VERSION, 4);
			pkt.freeRam = (uint32_t) (Utils::getFreeRam() / 1024);
			pkt.latency = NetworkManager::latency();
			pkt.ntpServer = NTP::getLastNTPServer();
			pkt.httpBaseAddress = HTTPClient::getBaseURL();
			pkt.platformName = getPlatformName();
			pkt.accelerometerName = Seismometer::getInstance()->getAccelerometerName();
			pkt.statProbeSpeed = Seismometer::getInstance()->getStatProbeSpeed();
			Log::d("Probe/sec: %i", pkt.statProbeSpeed);

			pkt.type = PKTTYPE_GETINFO_REPLY;
			sendPacket(pkt);
		}
#ifdef DEBUG
		case PKTTYPE_RESET:
		{
			unlink(DEFAULT_CONFIG_PATH);
			platformReboot();
		}
			break;
		case PKTTYPE_TRACE:
		{
			TraceAccumulator::setTrace(true);
		}
#endif
			break;
		default:
			break;
	}
}

// establish the connection through the CMD_INTERFACE_PORT port to interact with the mobile APP
bool CommandInterface::commandInterfaceInit() {
	bool ret = cmdc.listen(CMD_INTERFACE_PORT);
	cmdc.setNonblocking();
	if (!ret) {
		Log::e("Error during listening");
	}
	return ret;
}
