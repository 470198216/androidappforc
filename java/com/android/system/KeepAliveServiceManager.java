package com.android.system;

import android.os.ServiceManager;
import android.util.Log;

public class KeepAliveServiceManager {
    private static final String TAG = "KeepAliveServiceManager";
    private static final String SERVICE_NAME = "keepalive";

    public static void main(String[] args) {
        Log.i(TAG, "KeepAliveServiceManager starting");
        
        try {
            KeepAliveService service = new KeepAliveService();
            ServiceManager.addService(SERVICE_NAME, service);
            Log.i(TAG, "KeepAliveService registered as '" + SERVICE_NAME + "'");
            
            synchronized (KeepAliveServiceManager.class) {
                while (true) {
                    try {
                        KeepAliveServiceManager.class.wait();
                    } catch (InterruptedException e) {
                        Log.w(TAG, "Service manager interrupted");
                    }
                }
            }
        } catch (Throwable e) {
            Log.e(TAG, "Failed to start KeepAliveService", e);
        }
    }
}