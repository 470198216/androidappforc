package com.android.system;

import android.os.Binder;
import android.os.IBinder;
import android.os.Process;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public class KeepAliveService extends IKeepAliveService.Stub {
    private static final String TAG = "KeepAliveService";
    private static final String SOCKET_PATH = "/dev/socket/keepalived";

    private FileInputStream mInputStream;
    private FileOutputStream mOutputStream;

    public KeepAliveService() {
        Log.i(TAG, "KeepAliveService created");
    }

    private synchronized boolean connect() {
        if (mInputStream != null && mOutputStream != null) {
            return true;
        }

        try {
            FileDescriptor fd = Os.socket(OsConstants.AF_UNIX, OsConstants.SOCK_STREAM, 0);
            if (fd == null) {
                Log.e(TAG, "Failed to create socket");
                return false;
            }

            try {
                Os.connect(fd, SOCKET_PATH);
                mInputStream = new FileInputStream(fd);
                mOutputStream = new FileOutputStream(fd);
                Log.i(TAG, "Connected to keepalived daemon");
                return true;
            } catch (ErrnoException e) {
                Log.e(TAG, "Failed to connect: " + e.getMessage());
                try {
                    Os.close(fd);
                } catch (ErrnoException ignored) {
                }
                return false;
            }
        } catch (ErrnoException e) {
            Log.e(TAG, "Failed to create socket: " + e.getMessage());
            return false;
        }
    }

    private synchronized String sendCommand(String command) {
        if (!connect()) {
            return "ERROR - Cannot connect to keepalived daemon";
        }

        try {
            mOutputStream.write((command + "\n").getBytes());
            mOutputStream.flush();

            byte[] buffer = new byte[1024];
            int bytesRead = mInputStream.read(buffer);
            if (bytesRead > 0) {
                return new String(buffer, 0, bytesRead).trim();
            } else {
                return "ERROR - Empty response";
            }
        } catch (IOException e) {
            Log.e(TAG, "Communication error: " + e.getMessage());
            disconnect();
            return "ERROR - Communication failed: " + e.getMessage();
        }
    }

    private synchronized void disconnect() {
        if (mInputStream != null) {
            try {
                mInputStream.close();
            } catch (IOException ignored) {
            }
            mInputStream = null;
        }
        if (mOutputStream != null) {
            try {
                mOutputStream.close();
            } catch (IOException ignored) {
            }
            mOutputStream = null;
        }
    }

    @Override
    public String ping() {
        return sendCommand("PING");
    }

    @Override
    public String getStatus() {
        return sendCommand("STATUS");
    }

    @Override
    public int getCounter() {
        String response = sendCommand("COUNTER");
        try {
            if (response.startsWith("COUNTER ")) {
                return Integer.parseInt(response.substring(8).trim());
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "Failed to parse counter: " + response);
        }
        return -1;
    }

    @Override
    public int setInterval(int seconds) {
        String response = sendCommand("SET_INTERVAL " + seconds);
        if (response.startsWith("SET_INTERVAL OK")) {
            return seconds;
        }
        return -1;
    }

    @Override
    public String getHelp() {
        return sendCommand("HELP");
    }

    public void shutdown() {
        disconnect();
        Log.i(TAG, "KeepAliveService shutdown");
    }
}