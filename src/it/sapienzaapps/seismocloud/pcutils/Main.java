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
	private static final String macRegex = "^([A-Za-z0-9]{2}[\\-:]){5}[A-Za-z0-9]{2}$";

	public static void main(String[] args) {

		if(args.length == 0) {
			EventQueue.invokeLater(new Runnable() {
				@Override
				public void run() {
					GUIMain ex = new GUIMain();
					ex.setVisible(true);
				}
			});
			return;
		}

		if("discovery".equals(args[0])) {
			discovery(null);
		} else if("info".equals(args[0])) {
			info(args);
		} else if("reboot".equals(args[0])) {
			reboot(args);
		} else if("reset".equals(args[0])) {
			reset(args);
		} else if("trace".equals(args[0])) {
			trace(args);
		} else {
			System.err.println("Parameters: <discovery|info|reboot|reset|setgps> [macaddress|ipaddress] [lat] [lon]");
		}
	}

	private static void initsocket() throws IOException {
		ds = new DatagramSocket(62001);
		ds.setSoTimeout(5 * 1000);
	}

	private static DatagramPacket prepareSimplePacket(int command, InetAddress dst) throws IOException {
		if(dst == null) {
			dst = InetAddress.getByName("255.255.255.255");
		}
		byte[] pktbuf = new byte[252];
		pktbuf[0] = 'I';
		pktbuf[1] = 'N';
		pktbuf[2] = 'G';
		pktbuf[3] = 'V';
		pktbuf[4] = 0;
		pktbuf[5] = (byte)command;
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
			if(buf[0] == 'I' && buf[1] == 'N' && buf[2] == 'G' && buf[3] == 'V' && (buf[5] == 2 || buf[5] == 12)) {
				return pkt;
			} else {
				return null;
			}
		} catch(SocketTimeoutException e) {
			throw e;
		} catch(IOException e) {
			return null;
		}
	}

	private static void reboot(String[] args) {
		if(args.length < 2) {
			System.err.println("Missing MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		if(!args[1].matches(macRegex)) {
			System.err.println("Invalid MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		try {
			String mac = args[1].replace(":-", "");
			initsocket();

			InetAddress addr = getAddressFromMac(mac);

			if(addr == null) {
				System.err.println("Cannot find " + mac + " in LAN");
				ds.close();
				return;
			}

			ds.send(prepareSimplePacket(10, addr));

			ds.close();
		} catch(IOException e) {
			e.printStackTrace();
		}
	}

	private static void trace(String[] args) {
		if(args.length < 2) {
			System.err.println("Missing MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		if(!args[1].matches(macRegex)) {
			System.err.println("Invalid MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		try {
			String mac = args[1].replace(":-", "");
			initsocket();

			InetAddress addr = getAddressFromMac(mac);

			if(addr == null) {
				System.err.println("Cannot find " + mac + " in LAN");
				ds.close();
				return;
			}

			ds.send(prepareSimplePacket(14, addr));

			ds.close();
		} catch(IOException e) {
			e.printStackTrace();
		}
	}

	private static void reset(String[] args) {
		if(args.length < 2) {
			System.err.println("Missing MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		if(!args[1].matches(macRegex)) {
			System.err.println("Invalid MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		try {
			String mac = args[1].replace(":-", "");
			initsocket();

			InetAddress addr = getAddressFromMac(mac);

			if(addr == null) {
				System.err.println("Cannot find " + mac + " in LAN");
				ds.close();
				return;
			}

			ds.send(prepareSimplePacket(13, addr));

			ds.close();
		} catch(IOException e) {
			e.printStackTrace();
		}
	}

	private static InetAddress getAddressFromMac(String mac) throws IOException {
		ds.send(prepareSimplePacket(1, null));

		while(true) {
			DatagramPacket pkt;
			try {
				pkt = receivePacket();
			} catch(SocketTimeoutException e) {
				break;
			}
			if(pkt == null) continue;

			byte[] buf = pkt.getData();
			String devmac = String.format("%02x:%02x:%02x:%02x:%02x:%02x", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
			if(mac.equals(devmac)) {
				return pkt.getAddress();
			}
		}

		return null;
	}

	protected static void discovery(final IDiscoveryCallback callback) {
		try {
			initsocket();

			ds.send(prepareSimplePacket(1, null));

			while(true) {
				final DatagramPacket pkt;
				try {
					pkt = receivePacket();
				} catch(SocketTimeoutException e) {
					break;
				}
				if(pkt == null) continue;

				byte[] buf = pkt.getData();
				final String version = new String(buf, 12, 4, "UTF-8").trim();
				final String model = new String(buf, 16, 8, "UTF-8").trim();
				final String mac = String.format("%02x:%02x:%02x:%02x:%02x:%02x", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
				System.out.println("Arduino found at " + mac + " (" + pkt.getAddress() + ") model:" + model + " version:" + version);
				if(callback != null) {
					EventQueue.invokeLater(new Runnable() {
						@Override
						public void run() {
							callback.onDeviceFound(mac, model, version, pkt.getAddress());
						}
					});
				}
			}
			if(callback != null) {
				EventQueue.invokeLater(new Runnable() {
					@Override
					public void run() {
						callback.onDiscoveryEnded();
					}
				});
			}
		} catch(IOException e) {
			e.printStackTrace();
		}
	}

	private static void info(String[] args) {
		if(args.length < 2) {
			System.err.println("Missing MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		if(!args[1].matches(macRegex)) {
			System.err.println("Invalid MAC Address (specify as AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF)");
			return;
		}

		try {
			String mac = args[1].replace(":-", "");
			initsocket();

			InetAddress addr = getAddressFromMac(mac);

			if(addr == null) {
				System.err.println("Cannot find " + mac + " in LAN");
				ds.close();
				return;
			}

			ds.send(prepareSimplePacket(11, addr));

			while(true) {
				DatagramPacket pkt;
				try {
					pkt = receivePacket();
				} catch(SocketTimeoutException e) {
					break;
				}
				if(pkt == null) continue;

				byte[] buf = pkt.getData();

				String macstr = String.format("%02x:%02x:%02x:%02x:%02x:%02x", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);

				InetAddress syslogServer = InetAddress.getByAddress(new byte[] { buf[12], buf[13], buf[14], buf[15] });
				ByteBuffer uptimeBuffer = ByteBuffer.wrap(buf, 28, 4);
				ByteBuffer unixtimeBuffer = ByteBuffer.wrap(buf, 32, 4);
				ByteBuffer freeRamBuffer = ByteBuffer.wrap(buf, 40, 4);
				ByteBuffer latencyBuffer = ByteBuffer.wrap(buf, 44, 4);
				InetAddress ntpServer = InetAddress.getByAddress(new byte[] { buf[48], buf[49], buf[50], buf[51] });

				String version = new String(buf, 36, 4, "UTF-8");




				//String model = new String(buf, 16, 8, "UTF-8");


				System.out.println("MAC:" + macstr);
				System.out.println("IP:" + pkt.getAddress());
				System.out.println("syslogServer:" + syslogServer);
				System.out.println("Uptime:" + uptimeBuffer.getInt());
				System.out.println("Unix Time:" + unixtimeBuffer.getInt());
				System.out.println("Free RAM:" + freeRamBuffer.getInt());
				System.out.println("Latency:" + latencyBuffer.getFloat());
				System.out.println("ntpServer:" + ntpServer);
				//System.out.println("Model:" + model);
				System.out.println("Version:" + version);
				break;
			}

			ds.close();
		} catch(IOException e) {
			e.printStackTrace();
		}
	}
}
