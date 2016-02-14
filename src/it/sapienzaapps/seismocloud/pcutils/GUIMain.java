package it.sapienzaapps.seismocloud.pcutils;

import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.net.InetAddress;

/**
 * Created by enrico on 13/02/16.
 */
public class GUIMain extends JFrame {

	public GUIMain() {
		setTitle("SeismoCloud Test");
		setSize(300, 200);
		setLocationRelativeTo(null);
		setDefaultCloseOperation(WindowConstants.EXIT_ON_CLOSE);

		final JTextArea logArea = new JTextArea();
		logArea.setEditable(false);

		final JButton discoveryButton = new JButton("Discovery");
		discoveryButton.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				discoveryButton.setEnabled(false);
				new Thread(new Runnable() {
					   @Override
					   public void run() {
						   logArea.setText("Discovery started\n");
						   Main.discovery(new IDiscoveryCallback() {
							   @Override
							   public void onDeviceFound(String mac, String model, String version, InetAddress addr) {
								   logArea.append("Arduino found at " + mac + " (" + addr + ") model:" + model + " version:" + version+"\n");
							   }

							   @Override
							   public void onDiscoveryEnded() {
								   logArea.append("Discovery ended");
								   discoveryButton.setEnabled(true);
							   }
						   });
					   }
				   }
				).start();
			}
		});

		logArea.setPreferredSize(new Dimension(500, 200));

		JPanel panel = new JPanel();
		panel.setLayout(new GridLayout(2, 1));
		JPanel discoveryPanel = new JPanel();
		discoveryPanel.add(discoveryButton);
		panel.add(discoveryPanel);
		panel.add(logArea);
		add(panel);
		pack();
	}
}
