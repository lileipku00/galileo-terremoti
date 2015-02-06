#if ARDUINO < 153
#else
	#define __IS_GALILEO
#endif

#include <Ethernet.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SD.h>
#include <math.h>

#ifdef __IS_GALILEO
	#include <EthernetUdp.h>
	#include <sys/sysinfo.h>
	#include <signal.h>
	#include <stdlib.h>  // for ntp_alt.h
#endif

// accelerometer values
double pthresx = 0;
double pthresy = 0;
double pthresz = 0;
double nthresx = 0;
double nthresy = 0;
double nthresz = 0;

bool resetEthernet = false;
unsigned long freeRam();

unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long previousMillisNTP = 0;     // will store last time LED was updated
unsigned long pingIntervalCheckCounter = 0;
unsigned long calibrationInterval = 60*60*1000;
unsigned long prevMillisCalibration = 0;


#include "AcceleroMMA7361.h"
#include "GalileoLog.h"
#include "commons.h"
#include "ntp_alt.h"
#include "localstream.h"
#include "threshold.h"
#include "avg.h"
#include "httpconn.h"
#include "cfgupdate.h"

//definition freeRam method
#ifdef __IS_GALILEO
	unsigned long freeRam() {
		struct sysinfo s;
		sysinfo(&s);
		return s.freeram;
	}

	// static temp struct
	static struct RECORD ddl = {0, 0, 0, 0, 0, false};
	struct RECORD *db = &ddl;
	unsigned long currentMillis, milldelayTime;
#else
#if (ARDUINO >= 100)
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif
#endif

//definition of Accelerometer object
AcceleroMMA7361 accelero;
int currentHour = -1;

// return true if at least one of the axis is over the threshold
boolean isOverThresholdBasic(struct RECORD *db, struct TDEF *td) {
  return (db->valx > td->pthresx || db->valx < td->nthresx)
      || (db->valy > td->pthresy || db->valy < td->nthresy)
      || (db->valz > td->pthresz || db->valz < td->nthresz);
}

boolean isOverThresholdFixed(struct RECORD *db, struct TDEF *td) {
  return (abs(db->valx) > td->pthresx)
      || (abs(db->valy) > td->pthresy)
      || (abs(db->valz - gForce) > td->pthresz);
}

void checkSensore() 
{
  int valx, valy, valz;
  
  valx = accelero.getXAccel();
  valy = accelero.getYAccel();
  valz = accelero.getZAccel();
      
  TDEF td = { pthresx, pthresy, pthresz, nthresx, nthresy, nthresz };
  
  //struct RECORD *db = (struct RECORD*)malloc(sizeof(struct RECORD));
  db->ts = getUNIXTime();
  db->ms = getUNIXTimeMS();
  db->valx = getAvgX(valx);
  db->valy = getAvgY(valy);
  db->valz = getAvgZ(valz);
  //db->overThreshold = isOverThresholdBasic(db, &td);
  db->overThreshold = false;


  switch(thresholdAlghoritm) {
  	case Basic:
  		db->overThreshold = isOverThresholdBasic(db, &td);
  		break;

  	case Fixed:
  		db->overThreshold = isOverThresholdFixed(db, &td);
  		break;

  	default:  // Basic
  		db->overThreshold = isOverThresholdBasic(db, &td);
  }

  //debug_Axis(); // Debug Only
   
  if (internetConnected) {
  	sendValues(db);  // send the values of the accelerometer to the mobile APP (if the APP is listening)
  }
  
  // if the values of the accelerometer have passed the threshold
  //  or if an "event" is currently running
  if (db->overThreshold || inEvent == 1) {
    Serial.println(db->overThreshold?"overThreshold":"inEvent");
    if (internetConnected) {
      if (ledON) digitalWrite(10,HIGH);
      httpSendValues(db, &td);
      //Serial.println("IN EVENT - __CONNECTED__");
    }
    else {
      //saveToSDhttpSendValues();  // not yet implemented
      //free(db); // Memory leak debugged
      if (ledON) digitalWrite(10,LOW);
      if (logON) log("NOT CONNECTED");
      //Serial.println("freeing memory for db");
      //Serial.println("IN EVENT - BUT NOT CONNECTED");
      
      // STORE FILE ON SD CARD
      // sd = active
    }
  }
  else {
      //free(db); // Memory leak debugged
      if (ledON) digitalWrite(10,LOW);
      //Serial.println("freeing memory for db");
      //Serial.println("NOT IN EVENT");
   }
   
   
  db->ts = 0;
  db->ms = 0;
  db->valx = 0;
  db->valy = 0;
  db->valz = 0;
  db->overThreshold = false;

   
}

void debug_Axis() {  // Reading sensor to view what is measuring. For Debug Only
  int valx, valy, valz;
  
  //valx = accelero.getXAccel();
  //valy = accelero.getYAccel();
  //valz = accelero.getZAccel();

  if (debugON /*&& db->overThreshold*/) {
    Serial.print("Valori Accelerometro:  ");
    Serial.print(db->valx);
    Serial.print("   ");
    Serial.print(db->valy);
    Serial.print("   ");
    Serial.println(db->valz);
  }
}

// set up the ethernet connection per location;
// use the linux underneath the device to force manual DNS and so on
void setupEthernet() {
  switch (deviceLocation) {
	  case Colossus:  // Sapienza Colossus
	    ip = IPAddress(10, 10, 1, 101);
	    dns = IPAddress(151, 100, 17, 18);
	    gateway = IPAddress(10, 10, 1, 1);
	    subnet = IPAddress(255, 255, 255, 0);
            Ethernet.begin(mac, ip, dns, gateway); //vedere
	    timeServer = IPAddress(10, 10, 1, 1);
            system("ifconfig eth0 10.10.1.101 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
	    system("route add default gw 10.10.1.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
	    system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
	    break;

	  case Panizzi:  // Panizzi's room
	    ip = IPAddress(151, 100, 17, 143);
	    dns = IPAddress(151, 100, 17, 18);
	    gateway = IPAddress(151, 100, 17, 1);
	    subnet = IPAddress(255, 255 ,255 ,0);
            Ethernet.begin(mac, ip, dns, gateway);
	    timeServer = IPAddress(37, 247, 49, 133);
            system("ifconfig eth0 151.100.17.143 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
	    system("route add default gw 151.100.17.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
	    system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
            break;

	  case Home:  // Home
	    timeServer = IPAddress(132, 163, 4, 101);
	    if (isDhcpEnabled) {
	      boolean isDhcpWorking = false;
	      while (!isDhcpWorking) { // WARNING: add DHCP timeout
        /* Trying to get an IP address */
        if (Ethernet.begin(mac) == 0) {  // Error retrieving DHCP IP 
          if (debugON) Serial.println("Error while attempting to get an IP, retrying in 5 seconds...");
          delay(5000);
        }else {  // DHCP IP retireved successfull
          if (debugON) Serial.print("IP retrived successfully from DHCP: ");
          if (debugON) Serial.println(Ethernet.localIP());
          isDhcpWorking = true;
      }
          }
         break;
	    }
	    else { // Home Static Configuration 
        if (debugON) Serial.println("Static Configuration");
        if (logON) log("Static Configuration\n");
	      //add your static IP here
  	    ip = IPAddress(192, 168, 1, 36);
  	    dns = IPAddress(8,8,8,8);
  	    gateway = IPAddress(192,168,1,254);
        subnet = IPAddress(255, 255 ,255 ,0);
        // ARDUINO START CONNECTION		
	      Ethernet.begin(mac, ip, dns, gateway, subnet); // Static address configuration 
        //LINUX SYSTEM START CONNECTION
        system("ifconfig eth0 192.168.1.177 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
	      system("route add default gw 192.168.1.254 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
	      system("echo 'nameserver 8.8.8.8' > /etc/resolv.conf");  // add the GOOGLE DNS
				  
              //system("ifconfig eth0 192.168.1.36");  // fixed ip address to use the telnet connection
	      //system("ifconfig > /dev/ttyGS0");  // debug
              if (debugON) {
                Serial.print("Static IP: ");
            	Serial.println(Ethernet.localIP());
              }
              break;
            }// END - Home Static Configuration 
	    break;

	  default:  // like case 0, Sapienza Colossus
	    Ethernet.begin(mac, ip, dns, gateway);
	    system("ifconfig eth0 10.10.1.101 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
	    system("route add default gw 10.10.1.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
	    system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
      system("ifconfig > /dev/ttyGS0");  // debug
  }// END switch device location
}// END SetupEthernet()

// set up the ethernet connection per location;
// use the linux underneath the device to force manual DNS and so on
// void setupEthernet() {
	// switch (deviceLocation) {
			// case Colossus:  // Sapienza Colossus
				// ip = IPAddress(10, 10, 1, 101);
				// dns = IPAddress(151, 100, 17, 18);
				// gateway = IPAddress(10, 10, 1, 1);
				// subnet = IPAddress(255, 255, 255, 0);

				// Ethernet.begin(mac, ip, dns, gateway); //vedere
				// timeServer = IPAddress(10, 10, 1, 1);

				// system("ifconfig eth0 10.10.1.101 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
				// system("route add default gw 10.10.1.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
				// system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
				// break;

			// case Panizzi:  // Panizzi's room
				// ip = IPAddress(151, 100, 17, 143);
				// dns = IPAddress(151, 100, 17, 18);
				// gateway = IPAddress(151, 100, 17, 1);
				// subnet = IPAddress(255, 255 ,255 ,0);

				// Ethernet.begin(mac, ip, dns, gateway);
				// timeServer = IPAddress(37, 247, 49, 133);

				// system("ifconfig eth0 151.100.17.143 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
				// system("route add default gw 151.100.17.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
				// system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
				// break;

			// case Home:  // Home
				// timeServer = IPAddress(132, 163, 4, 101);
				// if (isDhcpEnabled) {
					// boolean isDhcpWorking = false;
					// while (!isDhcpWorking) { // WARNING: add DHCP timeout
						// /* Trying to get an IP address */
            // Serial.println("Trying get DHCP IP");
						// if (Ethernet.begin(mac) == 0) {
							// if (debugON) Serial.println("Error while attempting to get an IP, retrying in 5 seconds...");
							// delay(5000);
						// }
						// else {
							// if (debugON) Serial.print("IP retrived successfully from DHCP: ");
							// if (debugON) Serial.println(Ethernet.localIP());
							// isDhcpWorking = true;
						// }
					// }
				// }
				// else {
						//add your static IP here
						//ip = IPAddress(192, 168, 1, 36);
						// Ethernet.begin(mac); // controllare errore
						//system("ifconfig eth0 192.168.1.36");  // fixed ip address to use the telnet connection
						// system("ifconfig > /dev/ttyGS0");  // debug
				// }
				// break;

			// default:  // like case 0, Sapienza Colossus
				// Ethernet.begin(mac, ip, dns, gateway);
				// system("ifconfig eth0 10.10.1.101 netmask 255.255.255.0 up > /dev/ttyGS0 < /dev/ttyGS0");  // set IP and SubnetMask for the Ethernet
				// system("route add default gw 10.10.1.1 eth0 > /dev/ttyGS0 < /dev/ttyGS0");  // change the Gatway for the Ethernet
				// system("echo 'nameserver 151.100.17.18' > /etc/resolv.conf");  // add the DNS
	// }
// }

void setup() {
	#ifdef __IS_GALILEO
    // Fixing Arduino Galileo bug
    signal(SIGPIPE, SIG_IGN);
    // Workaround for Galileo (and other boards with Linux)
    system("/etc/init.d/networking restart");
    delay(1000);
    // Remove for production use
    system("telnetd -l /bin/sh");
  #endif
  //system("/etc/init.d/networking restart");
  //delay(1000);
  //system("telnetd -l /bin/sh");
  Serial.begin(9600);
  delay(500);

  Serial.println("#############INITIALIZING DEVICE#############\n");
  if (logON) log("#########INITIALIZING DEVICE##########\n");
  /* Calibrating Accelerometer */
  accelero.begin(/* 13, 12, 11, 10, */ A0, A1, A2);     // set the proper pin x y z
  //accelero.setSensitivity(LOW);                  // sets the sensitivity to +/-6G
  if (debugON) Serial.println("calibrate()");
  accelero.setAveraging(10);  // number of samples that have to be averaged
  accelero.calibrate();
  if (debugON) Serial.println("setAveraging(10)");
//  #ifdef __IS_GALILEO
//	  // Workaround for Galileo (and other boards with Linux)
//	  system("/etc/init.d/networking restart");
//          delay(1000);
//  #endif

  if (debugON) Serial.println("Setting up ethernet connection");
  // Config connction on Ethernet module

  // if (!doesFileExist(macAddressFilePath)) {
		// getMacAddressFromServer();
	// }
	// convertMACFromStringToByte();
  setupEthernet();

  internetConnected = isConnectedToInternet();
  delay(500); // wait internet connaction Status 
  Serial.print("STATUS CONNECTION: ");
  Serial.println(internetConnected ? "CONNECTED" : "NOT CONNECTED");
  delay(500);  
  
  if (ledON) {
  	pinMode(12, OUTPUT);
		if (internetConnected) digitalWrite(12,HIGH);
		digitalWrite(10,LOW);
		pinMode(10, OUTPUT);
		digitalWrite(10,LOW);
  }

  //system("cat /etc/resolv.conf > /dev/ttyGS0 < /dev/ttyGS0");  // DEBUG
  
  if (debugON) Serial.println("Forcing config update...");

  initConfigUpdates();
  
  if (debugON) Serial.println("Syncing NTP...");
  initNTP();
  // We need to set this AFTER ntp sync...
  lastCfgUpdate = getUNIXTime();
  
  if (debugON) Serial.println("EEPROM init");
  initEEPROM(forceInitEEPROM);
  
  if (debugON) Serial.println("UDP Command Socket init");
  commandInterfaceInit(); // open port for mobile app
  
  if (debugON) Serial.print("Free RAM: ");
  if (debugON) Serial.println(freeRam()); //debug
  
  if (debugON) Serial.println("\n#############INIZIALIZATION COMPLETE!#############");
  milldelayTime = millis();
}
// end SETUP

void loop() {
	currentMillis = millis();
	// debug only, check if the sketch is still running
	if ((currentMillis - previousMillis > checkInternetConnectionInterval) /*|| resetConnection*/) {
		internetConnected = isConnectedToInternet();
		if (!internetConnected) {
			if (ledON) digitalWrite(12,LOW);
      if(logON) log("networking restart - NOT CONNECTED FINTO!!!");
      if(debugON) Serial.println("networking restart FINTO!!!");
      // Workaround for Galileo (and other boards with Linux)
      //system("/etc/init.d/networking restart");
      delay(1000);
		}
		else {
			if (ledON) digitalWrite(12,HIGH);
        doConfigUpdates();
		}
                 
		if(logON)log("Still running");
		if (debugON) {
			Serial.print("Still running__INTERVAL: ");
			Serial.println(checkInternetConnectionInterval);
			Serial.print("STATUS CONNECTION: ");
			Serial.println(internetConnected?"CONNECTED":"NOT CONNECTED");
		}
		previousMillis = currentMillis;
	}

  
  // sync with the NTP server
	unsigned long currentMillisNTP = millis();
	if ((currentMillisNTP - previousMillisNTP > NTPInterval) && internetConnected && !resetEthernet) {
	  NTPdataPacket();
	  previousMillisNTP = currentMillisNTP;
    // doConfigUpdates(); // test test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	}
  
  // Check for calibration Sensor
  unsigned long currentMillisCalibration = millis();
	if ((currentMillisCalibration - prevMillisCalibration > calibrationInterval) || ForceCalibrationNeeded) {
     int cHour = (getUNIXTime() % 86400L) / 3600;
	   if (currentHour != cHour || ForceCalibrationNeeded ) {
	     if (debugON) Serial.println("checkCalibrationNeeded---------------------------------------");
	     checkCalibrationNeeded(accelero, cHour);
       currentHour = cHour;
       ForceCalibrationNeeded = false;
	  }
    prevMillisCalibration = currentMillisCalibration;
  }
  
  
	// sync with the NTP server
/* 	unsigned long currentMillisNTP = millis();
	if (currentMillisNTP - previousMillisNTP > NTPInterval) {
		NTPdataPacket();
    // check calibration sensor 
    int cHour = (getUNIXTime() % 86400L) / 3600;
		if (currentHour != cHour) {
			currentHour = cHour;
			if (debugON) Serial.println("checkCalibrationNeeded");
			checkCalibrationNeeded(accelero, cHour);
		}
		previousMillisNTP = currentMillisNTP;
	} */
  
/*   if (millis() - milldelayTime > checkSensoreInterval) {
  // Workaround for Galileo (and other boards with Linux)
    system("/etc/init.d/networking restart");
    delay(1000);
    milldelayTime = millis();
  } */
  

  //doNTPActions();
  //delay(50);
  if (millis() - milldelayTime > checkSensoreInterval) {
    // check mobile command
		checkCommandPacket();
    // read sensor values
		checkSensore();

		//testNTP();
		milldelayTime = millis();
  }
}
