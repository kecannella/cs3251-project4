package com.example.clientapp;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import android.os.Bundle;
import android.os.Environment;
import android.os.StrictMode;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.TextView;

public class MainActivity extends Activity {
	private Socket s;
	private OutputStream out;
	private InputStream in;
	private static final String SERVERNAME = "10.0.2.2";
	private static final int SERVERPORT = 2048;
	private static final int HASHLENGTH = 32;
	private Map<ByteBuffer, String> serverFiles;
	private int numServerFiles;
	private int numDesiredFiles;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.activity_main);
	    
		StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
		StrictMode.setThreadPolicy(policy); 
		
		try {
			serverFiles = new HashMap<ByteBuffer, String>();
			s = new Socket(SERVERNAME, SERVERPORT);
			
			out = s.getOutputStream();
			in = s.getInputStream();
			String state = Environment.getExternalStorageState();
			if (!Environment.MEDIA_MOUNTED.equals(state)) {
				AlertDialog errorAlert = new AlertDialog.Builder(this).create();
				errorAlert.setTitle("Fatal Error!");
				errorAlert.setMessage("No SD Card found!");
				errorAlert.setButton(AlertDialog.BUTTON_NEUTRAL, "OK", new DialogInterface.OnClickListener() {				
					@Override
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
					}
				});
				errorAlert.show();
			}
		} catch (UnknownHostException e) {
			AlertDialog errorAlert = new AlertDialog.Builder(this).create();
			errorAlert.setTitle("Fatal Error!");
			errorAlert.setMessage("Could not find the server!");
			errorAlert.setButton(AlertDialog.BUTTON_NEUTRAL, "OK", new DialogInterface.OnClickListener() {				
				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
			});
			errorAlert.show();
			e.printStackTrace();
		} catch (IOException e) {
			AlertDialog errorAlert = new AlertDialog.Builder(this).create();
			errorAlert.setTitle("Fatal Error!");
			errorAlert.setMessage("Could not connect to the server!");
			errorAlert.setButton(AlertDialog.BUTTON_NEUTRAL, "OK", new DialogInterface.OnClickListener() {				
				@Override
				public void onClick(DialogInterface dialog, int which) {
					dialog.dismiss();
				}
			});
			errorAlert.show();
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
				byte[] hash = new byte[HASHLENGTH];
				in.read(hash);
				int nameLength = readInt();
				String filename = readString(nameLength);
				serverFiles.put(ByteBuffer.wrap(hash), filename);
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
			Set<ByteBuffer> clientFiles = getFileList();
			ArrayList<ByteBuffer> neededFiles = new ArrayList<ByteBuffer>();
			for (Map.Entry<ByteBuffer, String> pair : serverFiles.entrySet()) {
				if (!clientFiles.contains(pair.getKey())) {
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
				sendInt(numDesiredFiles);
				for (ByteBuffer hash : neededFiles) {
					out.write(hash.array());
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
					readFile(filename, filesize);
					addOutput("Downloaded " + filename);
				}
				addOutput("Finished!");
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
				
				int namesize = readInt();
				while (namesize != 0) {
					String filename = readString(namesize);
					int filesize = readInt();
					readFile(filename, filesize);
					namesize = readInt();
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
		DataInputStream in = new DataInputStream(this.in);
		try {
			int result = in.readInt();
			Log.d("info", "read int " + result);
			return result;
		} catch (IOException e) {
			Log.d("error", e.toString());
			e.printStackTrace();
			return 0;
		}
	}
	
	public void sendInt(int output) {
		DataOutputStream out = new DataOutputStream(this.out);
		try {
			out.writeInt(output);
			out.flush();
		} catch (IOException e) {
			e.printStackTrace();
		}
		
	}
	
	public void readFile(String filename, int filesize) {
		try {
			File musicFolder = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC);
			musicFolder.mkdirs();
			File newMusicFile = new File(musicFolder, filename);
			FileOutputStream file = new FileOutputStream(newMusicFile.getPath());
			int totalbytes = 0;
			byte[] buffer = new byte[8192];
			while (totalbytes < filesize) {
				int bytesToRead = Math.min(8192, filesize - totalbytes);
				int bytesRead = in.read(buffer, 0, bytesToRead);
				file.write(buffer, 0, bytesRead);
				totalbytes += bytesRead;
			}
			file.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	public Set<ByteBuffer> getFileList() {
		Set<ByteBuffer> set = new HashSet<ByteBuffer>();
		File musicFolder = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC);
		musicFolder.mkdirs();
		File[] music = musicFolder.listFiles();
		for (File song : music) {
			Log.d("hashing", "hashing file " + song.getName());
			try {
				MessageDigest sha256 = MessageDigest.getInstance("SHA-256");
				InputStream in = new FileInputStream(song);
				DigestInputStream digestIn = new DigestInputStream(in, sha256);
				byte[] buffer = new byte[8192];
				while(digestIn.read(buffer) != -1) {}
				byte[] result = sha256.digest();
				set.add(ByteBuffer.wrap(result));
				digestIn.close();
			} catch (NoSuchAlgorithmException e) {
				e.printStackTrace();				
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
		return set;
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
