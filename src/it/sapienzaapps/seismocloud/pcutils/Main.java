package it.sapienzaapps.seismocloud.pcutils;

import java.awt.*;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;

public class Main {

	private static DatagramSocket ds;

	public static void main(String[] args) {
		discovery(null);
	}

	private static void initsocket() throws IOException {
		ds = new DatagramSocket(62001);
		ds.setSoTimeout(5 * 1000);
	}

	private static DatagramPacket prepareSimplePacket(int command, InetAddress dst) throws IOException {
		if (dst == null) {
			dst = InetAddress.getByName("255.255.255.255");
		}
		byte[] pktbuf = new byte[252];
		pktbuf[0] = 'I';
		pktbuf[1] = 'N';
		pktbuf[2] = 'G';
		pktbuf[3] = 'V';
		pktbuf[4] = 0;
		pktbuf[5] = (byte) command;
		DatagramPacket pkt = new DatagramPacket(pktbuf, pktbuf.length);
		pkt.setAddress(dst);
		pkt.setPort(62001);
		return pkt;
	}

	private static DatagramPacket receivePacket() throws SocketTimeoutException {
		try {
			byte[] pktbuf = new byte[252];
			DatagramPacket pkt = new DatagramPacket(pktbuf, pktbuf.length);
			ds.receive(pkt);
			byte[] buf = pkt.getData();
			if (buf[0] == 'I' && buf[1] == 'N' && buf[2] == 'G' && buf[3] == 'V' && (buf[5] == 2 || buf[5] == 12)) {
				return pkt;
			} else {
				return null;
			}
		} catch (SocketTimeoutException e) {
			throw e;
		} catch (IOException e) {
			return null;
		}
	}

	protected static void discovery(final IDiscoveryCallback callback) {
		try {
			initsocket();

			ds.send(prepareSimplePacket(1, null));

			while (true) {
				final DatagramPacket pkt;
				try {
					pkt = receivePacket();
				} catch (SocketTimeoutException e) {
					break;
				}
				if (pkt == null) continue;

				byte[] buf = pkt.getData();
				final String version = new String(buf, 12, 4, "UTF-8").trim();
				final String model = new String(buf, 16, 8, "UTF-8").trim();
				final String mac = String.format("%02x:%02x:%02x:%02x:%02x:%02x", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
				System.out.println("Arduino found at " + mac + " (" + pkt.getAddress() + ") model:" + model + " version:" + version);
				if (callback != null) {
					EventQueue.invokeLater(new Runnable() {
						@Override
						public void run() {
							callback.onDeviceFound(mac, model, version, pkt.getAddress());
						}
					});
				}
			}
			if (callback != null) {
				EventQueue.invokeLater(new Runnable() {
					@Override
					public void run() {
						callback.onDiscoveryEnded();
					}
				});
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
}
