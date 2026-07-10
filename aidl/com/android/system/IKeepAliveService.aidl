package com.android.system;

interface IKeepAliveService {
    String ping();
    String getStatus();
    int getCounter();
    int setInterval(int seconds);
    String getHelp();
}