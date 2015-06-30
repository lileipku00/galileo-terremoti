#include "cfgupdate.h"
#include "commons.h"
#include "httpconn.h"
#include "ntp_alt.h"

FILE *fp;
unsigned long lastCfgUpdate = 0;
unsigned long cfgUpdateInterval = 60;

long previousMillisConfig = 0;        // will store last time LED was updated
long intervalConfig = 1 * 60 * 1000;// 3 minuti

IPAddress getFromString(char *ipAddr) {
	char *p1 = strchr(ipAddr, '.');
	char *p2 = strchr(p1 + 1, '.');
	char *p3 = strchr(p2 + 1, '.');

	*p1 = 0;
	*p2 = 0;
	*p3 = 0;

	uint32_t d1 = atoi(ipAddr);
	uint32_t d2 = atoi(p1 + 1);
	uint32_t d3 = atoi(p2 + 1);
	uint32_t d4 = atoi(p3 + 1);

	return (uint32_t) (d1 * 16777216 + d2 * 65536 + d3 * 256 + d4);
}

// prepare buffer for config update request
int prepareConfigBuffer(char *buf) {
	// deviceid, lat, lon, pthresy, version, model
	Log::d("mac testo: %s", mac_string);
	return sprintf(buf, "deviceid=%s&lat=%s&lon=%s&version=%.2f&model=%s", mac_string, configGal.lat, configGal.lon,
				   configGal.version, configGal.model);
}

// ask config to server - New Da finire
boolean getConfigNew() {
	Log::i("getConfigNew()------------------ START ----------------------");
	boolean ret = false;
	if (client.connect(httpServer, 80)) {
		Log::d("Requesting CONFIG to: %s", httpServer);
		char rBuffer[300];
		int rsize = prepareConfigBuffer(rBuffer);  // prepare the info for the new entry to be send to DB
		// sendig request to server
		client.print("POST ");
		client.print(DEFAULT_HTTP_PATH);
		client.print("/alive.php");
		client.println(" HTTP/1.1");
		client.print("Host: ");
		client.println(httpServer);
		client.println("Content-Type: application/x-www-form-urlencoded");
		client.print("Content-Length: ");
		client.println(rsize);
		client.println("Connection: close"); // ???
		client.println("");
		client.println(rBuffer);

		Log::d("sending Buffer: %s", rBuffer);
		delay(100); // ATTENDERE ARRIVO RISPOSTA!!!
		unsigned long responseMill = millis();

		while (!client.available() && (millis() - responseMill < timeoutResponse)) { ; }
		if (millis() - responseMill > timeoutResponse) {
			Log::e("TIMEOUT SERVER CONNECTION- getConfigNew()");
		} else { // data arrived
			Log::d("Dati arrivati - getConfigNew()");
			memset(rBuffer, 0, 300 * sizeof(char));
			// Reading headers
			int s = getLine(client, rBuffer, 300);

			if (s & strncmp(rBuffer, "HTTP/1.1 200", 12) == 0) {  // if it is an HTTP 200 response
				int bodySize = 0;
				do {
					s = getLine(client, rBuffer, 300);
					if (strncmp(rBuffer, "Content-Length", 14) == 0) {
						char *separator = strchr(rBuffer, ':');
						if (*(separator + 1) == ' ') {
							separator += 2;
						}
						else {
							separator++;
						}
						bodySize = atoi(separator);
					}
				} while (s > 0);
				// Content after "\n" , there is body response --------------------
				int p = 0;
				bool exec_status = false;
				memset(rBuffer, 0, 300 * sizeof(char));
				do {
					p = getPipe(client, rBuffer, 300);
					if (p > 0) {
						size_t l;
						char *separator = strchr(rBuffer, ':');
						*separator = 0;
						char *argument = separator + 1;
						l = strlen(argument);
						if (strncmp(rBuffer, "server", 6) == 0) { // SERVER TO CONTACT
							free(httpServer);  // ?
							httpServer = (char *) malloc(l * sizeof(char));

							Log::i("Dimensione server: %i", l);
							Log::i("Argomento: #%s#", argument);

							if (httpServer != NULL && (l > 0)) {
								strncpy(httpServer, argument, l);
								httpServer[l] = '\0';
								Log::i("Server: %s", httpServer);
							} else {
								Log::e("Malloc FAILED - getConfigUpdates");
							}
						}
						else if (strncmp(rBuffer, "ntpserver", 9) == 0) { // Ntpserver
							Log::d(argument);
							timeServer = getFromString(argument);
						}
						else if (strncmp(rBuffer, "script", 6) == 0) { // Check for executing script
							if (strlen(argument) > 0) {
								char scriptTest[100];
								size_t len = strlen(argument);
								strncpy(scriptTest, argument, len);
								scriptTest[len] = '\0';
								createScript("/media/realroot/script.sh", scriptTest);
								exec_status = true;
								Log::d("Script Creation...");
							}
							Log::d("Script length: %i", strlen(argument));
							Log::d("Script: %s", argument);
						}
						else if (strncmp(rBuffer, "path", 4) == 0) { // Check for downloading file
							size_t len = strlen(argument);
							if (len > 0) {
								char pathTest[300];
								char pathScriptDownload[300];
								strncpy(pathTest, argument, len); // remote peth for file downloading
								pathTest[len] = '\0';
								sprintf(pathScriptDownload, download_scriptText, pathTest);

								Log::i("pathTest: %s", pathTest);
								Log::i("pathScriptDownload: %s", pathScriptDownload);

								createScript(NULL,
											 pathScriptDownload); // creation script for download a file from the path(internet resource)
								Log::d("execScript for Download....");
								execScript(script_path); // executing download of the file
								delay(1000);
							}
							Log::d("Path length: %i", strlen(argument));
							Log::d("path: %s", argument);
						}
					}
				} while (p > 0);
				if (exec_status) { // check for executing script command
					execScript("/media/realroot/script.sh");
					Log::d("execScript.... /media/realroot/script.sh");
					for (int x = 0; x < 3; x++) {
						resetBlink(0);
					}
				}
				ret = true;
			}
			else { // not 200 response
				Log::d("Error in reply: %s", rBuffer);
			}
		} // end data arrived
	} else { // impossible to contact the server
		client.stop();
		errors_connection++;
		Log::e("getConfigNew() - connessione fallita");
	}


	while (client.connected()) {
		Log::i("disconnecting.");
		client.stop();
	}

	Log::d("Still running Config Update");
	if (isConnectedToInternet()) {
		Log::d("isConnectedToInternet");
	}
	Log::d("lastCfgUpdate: %ld", lastCfgUpdate);
	Log::d("cfgUpdateInterval: %ld", cfgUpdateInterval);
	Log::d("getUNIXTime(): ", getUNIXTime());
	if (!ret)
		Log::e("getConfigNew() Update ERROR!!!");
	Log::i("getConfigNew()------------------ EXIT ----------------------");
	return ret;
}

// get the HTTP Server(default if not) and NTP Server
void initConfigUpdates() {
	httpServer = (char *) malloc(strlen(DEFAULT_HTTP_SERVER) * sizeof(char));
	if (httpServer != NULL) {
		strcpy(httpServer, DEFAULT_HTTP_SERVER);
	} else {
		Log::e("Malloc FAILED - getConfigUpdates");
	}

	if (internetConnected && start) { // get config onli if Galileo is connected and lat/lon are setted
		boolean ret = getConfigNew();
		int nTimes = 0;
		while (!ret && (nTimes < 5)) {
			nTimes++;
			Log::d("Configuration update failed, retrying in 3 seconds...");
			delay(3000);
			ret = getConfigNew();
		}
		if (nTimes >= 5)
			Log::e("getConfigNew()  -  failed!!!!!");
	}
}

