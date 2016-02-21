package de.badaix.snapcast.utils;

/**
 * Created by johannes on 19.01.16.
 */

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import java.net.InetAddress;

public class NsdHelper {

    private static final String TAG = "NsdHelper";
    private static NsdHelper mInstance;
    private String serviceName;
    private String serviceType;
    //    private static final String NSD_SERVICE_TYPE = "_http._tcp.";
    private int mPort;
    private InetAddress mHost;
    private NsdManager mNsdManager;
    private android.net.nsd.NsdManager.DiscoveryListener mDiscoveryListener = null;
    private android.net.nsd.NsdManager.ResolveListener mResolveListener = null;
    private Context mContext;
    private NsdHelperListener listener = null;

    private NsdHelper(Context context) {
        mContext = context;
    }

    public static NsdHelper getInstance(Context context) {
        if (mInstance == null) {
            mInstance = new NsdHelper(context);
        } else {
            mInstance.mContext = context;
        }
        return mInstance;
    }

    public void startListening(String serviceType, String serviceName, NsdHelperListener listener) {
        stopListening();
        this.listener = listener;
        this.serviceName = serviceName;
        this.serviceType = serviceType;
        initializeResolveListener();
        initializeDiscoveryListener();
        mNsdManager = (NsdManager) mContext.getSystemService(Context.NSD_SERVICE);
        mNsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
//        mNsdManager.discoverServices("_snapcast-jsonrpc._tcp.", NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
    }

    public void stopListening() {
        if (mDiscoveryListener != null) {
            try {
                mNsdManager.stopServiceDiscovery(mDiscoveryListener);
            } finally {
            }
            mDiscoveryListener = null;
        }
    }

    private void initializeResolveListener() {
        mResolveListener = new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
                Log.d(TAG, "Resolve failed");
            }

            @Override
            public void onServiceResolved(NsdServiceInfo serviceInfo) {
                NsdServiceInfo info = serviceInfo;
                mHost = info.getHost();
                mPort = info.getPort();
                Log.d(TAG, "Service resolved :" + mHost + ":" + mPort);
                listener.onResolved(NsdHelper.this, serviceInfo);
            }
        };
    }

    private void initializeDiscoveryListener() {
        mDiscoveryListener = new NsdManager.DiscoveryListener() {
            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                Log.d(TAG, "Discovery failed");
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                Log.d(TAG, "Stopping discovery failed");
            }

            @Override
            public void onDiscoveryStarted(String serviceType) {
                Log.d(TAG, "Discovery started");
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                Log.d(TAG, "Discovery stopped");
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                NsdServiceInfo info = serviceInfo;
                Log.d(TAG, "Service found: " + info.getServiceName());
                if (info.getServiceName().equals(serviceName))
                    mNsdManager.resolveService(info, mResolveListener);
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                NsdServiceInfo info = serviceInfo;
                Log.d(TAG, "Service lost: " + info.getServiceName());
            }
        };
    }

    public interface NsdHelperListener {
        void onResolved(NsdHelper nsdHelper, NsdServiceInfo serviceInfo);
    }
}

