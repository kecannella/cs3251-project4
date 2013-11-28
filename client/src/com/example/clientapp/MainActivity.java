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
import java.util.concurrent.ExecutionException;

import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.StrictMode;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.SeekBar;
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
		
		SeekBar seekBar = (SeekBar)findViewById(R.id.cap_slider); 
		final TextView seekBarValue = (TextView)findViewById(R.id.cap_value); 

	    seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){ 
			@Override 
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) { 
				if (progress >= 1000000)
					seekBarValue.setText("Cap amount: " + String.valueOf(progress/1000000) + "MB");
				else
					seekBarValue.setText("Cap amount: " + String.valueOf(progress/1000) + "KB");
			} 
			
			@Override 
			public void onStartTrackingTouch(SeekBar seekBar) { 
			} 
			
			@Override 
			public void onStopTrackingTouch(SeekBar seekBar) { 
			} 
		}); 
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}
	
	public void doList(View view) {
		new ListTask().execute((Void) null);
	}
	
	public void doDiff(View view) {		
		new DiffTask().execute((Void) null);
	}
	
	public void doPull(View view) {
		new PullTask().execute((Void) null);
	}
	
	public void doCap(View view) {
		new CapTask().execute((Void) null);
	}
	
	public void doLeave(View view) {
		new LeaveTask().execute((Void) null);
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
			Log.d("info", "Finished reading file " + filename);
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
	
	private class ListTask extends AsyncTask<Void, String, Void> {

		@Override
		protected void onPreExecute() {
			setOutput("Server Files:");
		}
		
		@Override
		protected Void doInBackground(Void... params) {
			try {
				synchronized(s) {
					out.write(MessageType.LIST.getValue());
					out.flush();				
					numServerFiles = readInt();
					publishProgress(numServerFiles + " files");
					
					serverFiles.clear();
					for (int i = 0; i < numServerFiles; ++i) {
						byte[] hash = new byte[HASHLENGTH];
						in.read(hash);
						int nameLength = readInt();
						String filename = readString(nameLength);
						serverFiles.put(ByteBuffer.wrap(hash), filename);
						publishProgress(filename);
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
			}
			return null;
		}
		
		@Override
		protected void onProgressUpdate(String... progress) {
			addOutput(progress[0]);
		}
		
		@Override
		protected void onPostExecute(Void result) {
			addOutput("Done!");
		}
	}
	
	private class DiffTask extends AsyncTask<Void, String, Void> {
		AsyncTask<Void, String, Void> prevTask = null;

		@Override
		protected void onPreExecute() {		
			if (numServerFiles == 0) {
				prevTask = new ListTask().execute((Void) null);
			} else {
				setOutput("Needed Files:");
			}
		}
		
		@Override
		protected Void doInBackground(Void... params) {			
			try {
				if (prevTask != null) {
					prevTask.get();
					publishProgress("Needed Files: ");
				}
				synchronized(s) {
					Set<ByteBuffer> clientFiles = getFileList();
					ArrayList<ByteBuffer> neededFiles = new ArrayList<ByteBuffer>();
					for (Map.Entry<ByteBuffer, String> pair : serverFiles.entrySet()) {
						if (!clientFiles.contains(pair.getKey())) {
							neededFiles.add(pair.getKey());
							publishProgress(pair.getValue());
						}
					}
					numDesiredFiles = neededFiles.size();
					if (numDesiredFiles == 0) {
						publishProgress("All files up to date");
					}	else {
						out.write(MessageType.DIFF.getValue());
						out.flush();
						sendInt(numDesiredFiles);
						for (ByteBuffer hash : neededFiles) {
							out.write(hash.array());
						}
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
			} catch (InterruptedException e) {
				e.printStackTrace();
			} catch (ExecutionException e) {
				e.printStackTrace();
			}
			return null;
		}

		@Override
		protected void onProgressUpdate(String... progress) {
			addOutput(progress[0]);
		}
		
		@Override
		protected void onPostExecute(Void result) {
			addOutput("Done!");
		}
		
	}
	
	private class PullTask extends AsyncTask<Void, String, Void> {
		AsyncTask<Void, String, Void> prevTask = null;
		
		@Override
		protected void onPreExecute() {
			if (numDesiredFiles == 0) {
				prevTask = new DiffTask().execute((Void) null);
			} else {
				setOutput("Pulling Files:");
			}
		}
		
		@Override
		protected Void doInBackground(Void... params) {			
			try {
				if (prevTask != null) {
					prevTask.get();
					publishProgress("Pulling Files:");
				}
				if (numDesiredFiles != 0) {
					synchronized(s) {
						out.write(MessageType.PULL.getValue());
						out.flush();				
						for (int i = 0; i < numDesiredFiles; ++i) {
							int namesize = readInt();
							String filename = readString(namesize);
							int filesize = readInt();
							readFile(filename, filesize);
							publishProgress("Downloaded " + filename);
						}
						numDesiredFiles = 0;
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
			} catch (InterruptedException e) {
				e.printStackTrace();
			} catch (ExecutionException e) {
				e.printStackTrace();
			}
			return null;
		}

		@Override
		protected void onProgressUpdate(String... progress) {
			addOutput(progress[0]);
		}
		
		@Override
		protected void onPostExecute(Void result) {
			addOutput("Done!");
		}
		
	}
	
	private class CapTask extends AsyncTask<Void, String, Void> {
		AsyncTask<Void, String, Void> prevTask = null;
		int cap;

		@Override
		protected void onPreExecute() {
			SeekBar seekBar = (SeekBar)findViewById(R.id.cap_slider); 
			this.cap = seekBar.getProgress();
			if (numDesiredFiles == 0) {
				prevTask = new DiffTask().execute((Void) null);
			} else {
				setOutput("Pulling Files with " + cap + " Byte Cap:");
			}
		}
		
		@Override
		protected Void doInBackground(Void... params) {		
			try {
				if (prevTask != null) {
					prevTask.get();
					publishProgress("Pulling Files with " + cap + " Byte Cap:");
				}
				if (numDesiredFiles != 0) {
					synchronized(s) {
						out.write(MessageType.CAP.getValue());
						out.flush();
						
						sendInt(cap);
						Log.d("info", "cap size: " + cap);
						
						int namesize = readInt();
						while (namesize != 0) {
							String filename = readString(namesize);
							int filesize = readInt();
							Log.d("info", "filename: " + filename + " size: " + filesize);
							readFile(filename, filesize);
							namesize = readInt();
							publishProgress("Downloaded " + filename);
						}
						numDesiredFiles = 0;
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
			} catch (InterruptedException e) {
				e.printStackTrace();
			} catch (ExecutionException e) {
				e.printStackTrace();
			}
			return null;
		}

		@Override
		protected void onProgressUpdate(String... progress) {
			addOutput(progress[0]);
		}
		
		@Override
		protected void onPostExecute(Void result) {
			addOutput("Done!");
		}
		
	}
	
	private class LeaveTask extends AsyncTask<Void, String, Void> {

		@Override
		protected void onPreExecute() {
			setOutput("Leaving");
		}
		
		@Override
		protected Void doInBackground(Void... params) {
	    	try {
	    		synchronized(s) {
		    		out.write(MessageType.LEAVE.getValue());
					out.flush();
		   		
					s.close();
	    		}
	    	} catch (IOException e) {
				e.printStackTrace();
			}
			return null;
		}
		
		@Override
		protected void onPostExecute(Void result) {
			finish();
		}
		
	}
}
