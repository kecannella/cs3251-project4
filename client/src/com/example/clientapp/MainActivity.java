package com.example.clientapp;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import android.os.Bundle;
import android.os.StrictMode;
import android.app.Activity;
import android.util.Log;
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
	private int numDesiredFiles;

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
				int nameLength = readInt();
				String filename = readString(nameLength);
				serverFiles.put(hash, filename);
				addOutput(filename);
			}				
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	public void doDiff(View view) {
		if (numServerFiles == 0) {
			setOutput("Need to perform list");
			doList(view);
		}
		try {
			setOutput("Needed Files:");
			String directory = ""; // TODO
			Map<String, String> clientFiles = getFileList(directory);
			ArrayList<String> neededFiles = new ArrayList<String>();
			for (Map.Entry<String, String> pair : serverFiles.entrySet()) {
				if (!clientFiles.containsKey(pair.getKey())) {
					neededFiles.add(pair.getKey());
					addOutput(pair.getValue());
				}
			}
			numDesiredFiles = neededFiles.size();
			if (numDesiredFiles == 0) {
				addOutput("All files up to date");
			}
			else {
				out.write(MessageType.DIFF.getValue());
				out.flush();
				for (String hash : neededFiles) {
					sendString(hash);
				}
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
		
	}
	
	public void doPull(View view) {
		if (numDesiredFiles == 0) {
			setOutput("Need to perform diff");
			doDiff(view);
		}
		if (numDesiredFiles != 0) {
			try {
				setOutput("Pulling Files:");
				out.write(MessageType.PULL.getValue());
				out.flush();
				
				for (int i = 0; i < numDesiredFiles; ++i) {
					int namesize = readInt();
					String filename = readString(namesize);
					int filesize = readInt();
					// TODO setup directory for filename?
					readFile(filename, filesize);
				}
				numDesiredFiles = 0;
				
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}
	
	public void doCap(View view) {
		if (numDesiredFiles == 0) {
			setOutput("Need to perform diff");
			doDiff(view);
		}
		if (numDesiredFiles != 0) {
			try {
				setOutput("Pulling Files with cap:");
				out.write(MessageType.CAP.getValue());
				out.flush();
				
				int numFiles = readInt();
				for (int i = 0; i < numFiles; ++i) {
					int namesize = readInt();
					String filename = readString(namesize);
					int filesize = readInt();
					// TODO setup directory for filename?
					readFile(filename, filesize);
				}
				numDesiredFiles = 0;
				
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}
	
	public void doLeave(View view) {
		setOutput("Leaving");
    	try {
    		out.write(MessageType.LEAVE.getValue());
			out.flush();
   		
			s.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		this.finish();
	}
	
	public String readString(int length) {
		Log.d("info", "reading string length " + length);
		try {
			byte [] buffer = new byte[length];
			in.read(buffer);
			String result = new String(buffer);
			Log.d("info", "read string " + result);
			return result;
		} catch (IOException e) {
			Log.d("error", e.toString());
			e.printStackTrace();
			return "";
		}
	}
	
	public void sendString(String s) {
		try {
			out.write(s.getBytes());
			out.flush();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	public int readInt() {
		Log.d("info", "reading int");
		try {
			int result = (in.read()) | (in.read() << 8 ) | (in.read() << 18) | (in.read() << 24);
			Log.d("info", "read int " + result);
			return result;
		} catch (IOException e) {
			Log.d("error", e.toString());
			e.printStackTrace();
			return 0;
		}
	}
	
	public void sendInt(int output) {
		try {
			out.write((output) & 0xFF);
			out.write((output >> 8) & 0xFF);
			out.write((output >> 16) & 0xFF);
			out.write((output >> 24) & 0xFF);
			out.flush();
		} catch (IOException e) {
			e.printStackTrace();
		}
		
	}
	
	public void readFile(String filename, int filesize) {
		try {
			FileOutputStream file = new FileOutputStream(filename);
			int bytes, totalbytes = 0;
			while (totalbytes < filesize) {
				if (filesize - totalbytes < 4096) bytes = filesize - totalbytes;
				else bytes = 4096;
				byte[] buffer = new byte[bytes];
				in.read(buffer);
				file.write(buffer);
				totalbytes += bytes;
			}
			file.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	public Map<String,String> getFileList(String directory) {
		Map<String, String> map = new HashMap<String, String>();
		// TODO populate
		return map;
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
