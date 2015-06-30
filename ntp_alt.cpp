#include "ntp_alt.h"

#include "config.h"
#include "commons.h"

unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

// An UDP instance to let us send and receive packets over UDP
static char cmd0[30] = "";
static char cmd1[] = "/bin/date ";
static char cmd2[] = "/bin/date -s @";
static char bufSTR[13];

// conversion date from epoch
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

#define SECONDS_PER_DAY 86400L
#define GMT 0 // Time Zone is managed on the mobile app side

#define SECONDS_FROM_1970_TO_2000 946684800
uint8_t yOff, m, d, hh, mm, ss;  // data

/* *** OLD NTP.H *** START */
unsigned long _unixTimeTS = 0;
unsigned long _unixTimeUpdate = 0;
/* *** OLD NTP.H *** END */

// workaround for Galileo
unsigned long fixword(byte b1, byte b2) {
#ifdef __IS_GALILEO
	return (b1 << 8) | b2;
#else
	return word(b1, b2);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// utility code to get Human Date From epoch

const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
EthernetUDP UDP_as_NTP;


void dateGalileo(uint32_t t) {
	t -= SECONDS_FROM_1970_TO_2000;  // bring to 2000 timestamp from 1970

	ss = t % 60;
	t /= 60;
	mm = t % 60;
	t /= 60;
	hh = t % 24;
	uint16_t days = t / 24;
	uint8_t leap;
	for (yOff = 0; ; ++yOff) {
		leap = yOff % 4 == 0;
		if (days < 365 + leap)
			break;
		days -= 365 + leap;
	}
	for (m = 1; ; ++m) {
		uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
		if (leap && m == 2)
			++daysPerMonth;
		if (days < daysPerMonth)
			break;
		days -= daysPerMonth;
	}
	d = days + 1;
	// Displaying Current Date and Time

	Log::d("%i:%i:%i %i/%i%/%i", hh, mm, ss, d, m, 2000 + yOff);
}
// end conversion date from epoch

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	UDP_as_NTP.beginPacket(address, 123); //NTP requests are to port 123
	UDP_as_NTP.write(packetBuffer, NTP_PACKET_SIZE);
	UDP_as_NTP.endPacket();
}

bool NTPdataPacket() {
	bool NTPsynced = false;
	memset(cmd0, 0, 30);
	memset(bufSTR, 0, 13);
	// while (!NTPsynced) {
	if (internetConnected) {
		sendNTPpacket(timeServer); // send an NTP packet to a time server

		// wait to see if a reply is available
		delay(500);
		unsigned long responseMill = millis();
		// WAIT FOR SERVER RESPONCE
		while (!NTPsynced && (millis() - responseMill < timeoutResponse)) {
			if (UDP_as_NTP.parsePacket() >= NTP_PACKET_SIZE) {
				NTPsynced = true;
				// We've received a packet, read the data from it
				UDP_as_NTP.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer

				//the timestamp starts at byte 40 of the received packet and is four bytes,
				// or two words, long. First, esxtract the two words:

				unsigned long highWord = fixword(packetBuffer[40], packetBuffer[41]);
				unsigned long lowWord = fixword(packetBuffer[42], packetBuffer[43]);
				// combine the four bytes (two words) into a long integer
				// this is NTP time (seconds since Jan 1 1900):
				unsigned long secsSince1900 = highWord << 16 | lowWord;
				Log::d("Seconds since Jan 1 1900 = %ld", secsSince1900);

				// now convert NTP time into everyday time:
				// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
				const unsigned long seventyYears = 2208988800UL;
				// subtract seventy years:
				unsigned long epoch = secsSince1900 - seventyYears + GMT;
				_unixTimeTS = (epoch); //* 1000UL;
				_unixTimeUpdate = millis();
				// print Unix time:
				Log::d("Unix time = %i", epoch);
				//dateGalileo(epoch);
				delay(50);
				strcat(cmd0, cmd2);
				snprintf(bufSTR, 12, "%lu", epoch);
				strcat(cmd0, bufSTR);

				Log::d("Date and Time Command: %s", cmd0);
				return 1;
			} else {
				Log::e("ERROR NTP PACKET NOT RECEIVED");
			}
		}
	} else {
		Log::e("ERROR INTERNET NOT PRESENT IN: NTPdataPacket()");
		//  Internet not connected while try to sync with NTP
	}
	return 0;
}

// Set date and time to NTP's retrieved one
void execSystemTimeUpdate() {
	char buf[64];
	FILE *ptr;
	Log::d("COMANDO: %s", cmd0);
	if ((ptr = popen((char *) cmd0, "r")) != NULL) {
		while (fgets(buf, 64, ptr) != NULL) {
			Log::d(buf);
		}
		(void) pclose(ptr);
	} else {
		Log::d("error popen NTP init");
	}
}

// Connect to NTP server and set System Clock
void initNTP() {
	UDP_as_NTP.begin(localPort);
	delay(1000);
	if (NTPdataPacket()) {
		execSystemTimeUpdate();
	} else {
		Log::d("Errore NTPdataPacket() ");
	}
}

// for debug purpose only
void testNTP() {
	char *cmdP = "/bin/date +%F%t%T";
	char buf[64];
	FILE *ptr;

	if ((ptr = popen(cmdP, "r")) != NULL) {
		while (fgets(buf, 64, ptr) != NULL) {
			Log::d(buf);
		}
	}
	(void) pclose(ptr);
	delay(1000);
}

unsigned long getUNIXTime() {
	unsigned long diff = millis() - _unixTimeUpdate;
	return (_unixTimeTS + (diff / 1000));
}

unsigned long int getUNIXTimeMS() {
	unsigned long diff = millis() - _unixTimeUpdate;
	return (((_unixTimeTS) + (diff /= 1000) + (diff % 1000 >= 0 ? 1 : 0)));
}