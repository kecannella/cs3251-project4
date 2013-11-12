package com.example.clientapp;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.Map;

import android.os.Bundle;
import android.os.StrictMode;
import android.app.Activity;
import android.view.Menu;
import android.view.View;
import android.widget.TextView;

public class MainActivity extends Activity {
	private Socket s;
	private OutputStream out;
	private InputStream in;
	private static final String SERVERNAME = "shuttle1.cc.gatech.edu";
	private static final int SERVERPORT = 2048;
	private static final int HASHLENGTH = 32;
	private Map<String, String> serverFiles;
	private int numServerFiles;
	//private int numDesiredFiles;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.activity_main);
	    
		StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
		StrictMode.setThreadPolicy(policy); 
		
		try {
			serverFiles = new HashMap<String, String>();
			s = new Socket(SERVERNAME, SERVERPORT);
			out = s.getOutputStream();
			in = s.getInputStream();
		} catch (UnknownHostException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}
	
	
	public void doList(View view) {
		try {
			setOutput("Server Files:");
			
			out.write(MessageType.LIST.getValue());
			out.flush();
			
			numServerFiles = readInt();
			addOutput(numServerFiles + " files");
			
			serverFiles.clear();
			for (int i = 0; i < numServerFiles; ++i) {
				String hash = readString(HASHLENGTH);
				// int nameLength = readInt();
				String filename = "";
				// String filename = readString(nameLength);
				serverFiles.put(hash, filename);
				addOutput(filename);
			}			
			
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	public void doDiff(View view) {
		setOutput("Needed Files:");
	}
	
	public void doPull(View view) {
		setOutput("Pulling Files:");
	}
	
	public void doCap(View view) {
		setOutput("Pulling Files with Cap:");
	}
	
	public void doLeave(View view) {
		setOutput("Leaving");
    	try {
    		out.write(MessageType.LEAVE.getValue());
			s.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		this.finish();
	}
	
	public String readString(int length) {
		try {
			byte [] buffer = new byte[length];
			in.read(buffer);
			String result = new String(buffer);
			return result;
		} catch (IOException e) {
			e.printStackTrace();
			return "";
		}
	}
	
	public int readInt() {
		try {
			int result = (in.read()) | (in.read() << 8 ) | (in.read() << 18) | (in.read() << 24);
			return result;
		} catch (IOException e) {
			e.printStackTrace();
			return 0;
		}
	}
	
	public void setOutput(String out) {
		TextView output = (TextView)findViewById(R.id.output);
		output.setText(out);
	}
	
	public void addOutput(String out) {
		TextView output = (TextView)findViewById(R.id.output);
		output.setText(output.getText() + "\n" + out);		
	}

	
	private enum MessageType {
		  LIST((byte)1),
		  DIFF((byte)2),
		  PULL((byte)3),
		  CAP((byte)4),
		  LEAVE((byte)5);

		  private byte value;    

		  private MessageType(byte value) {
		    this.value = value;
		  }

		  public byte getValue() {
		    return value;
		  }
	}
}
