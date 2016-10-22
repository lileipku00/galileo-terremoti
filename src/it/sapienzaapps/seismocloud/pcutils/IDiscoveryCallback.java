package it.sapienzaapps.seismocloud.pcutils;

import java.net.InetAddress;

/**
 * Created by enrico on 13/02/16.
 */
public interface IDiscoveryCallback {

    void onDeviceFound(String mac, String model, String version, InetAddress addr);

    void onDiscoveryEnded();

}
