package de.badaix.snapcast;

import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.net.Uri;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.appindexing.Action;
import com.google.android.gms.appindexing.AppIndex;
import com.google.android.gms.common.api.GoogleApiClient;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.ServerInfo;
import de.badaix.snapcast.control.Volume;

public class MainActivity extends ActionBarActivity implements View.OnClickListener, TcpClient.TcpClientListener, ClientInfoItem.ClientInfoItemListener {

    private static final String TAG = "Main";

    private Button buttonScan;
    private Button buttonStart;
    private Button buttonStop;
    private Button button;
    private TextView tvHost;
    private TextView tvInfo;
    private CheckBox cbScreenWakelock;
    private ListView lvClient;
    private NsdManager.DiscoveryListener mDiscoveryListener;
    private NsdManager mNsdManager = null;
    private String host = "";
    private int port = 1704;
    private TcpClient tcpClient;
    private ServerInfo serverInfo;
    private ClientInfoAdapter clientInfoAdapter;

    /**
     * ATTENTION: This was auto-generated to implement the App Indexing API.
     * See https://g.co/AppIndexing/AndroidStudio for more information.
     */
    private GoogleApiClient client;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        buttonScan = (Button) findViewById(R.id.buttonScan);
        buttonStart = (Button) findViewById(R.id.buttonStart);
        buttonStop = (Button) findViewById(R.id.buttonStop);
        button = (Button) findViewById(R.id.button);
        tvHost = (TextView) findViewById(R.id.tvHost);
        tvInfo = (TextView) findViewById(R.id.tvInfo);
        cbScreenWakelock = (CheckBox) findViewById(R.id.cbScreenWakelock);
        cbScreenWakelock.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                if (b)
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });

        lvClient = (ListView) findViewById(R.id.lvClient);

        buttonScan.setOnClickListener(this);
        buttonStart.setOnClickListener(this);
        buttonStop.setOnClickListener(this);
        button.setOnClickListener(this);

        AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            String rate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            String size = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            tvInfo.setText("Preferred buffer size: " + size + "\nPreferred sample rate: " + rate);
        }

        serverInfo = new ServerInfo();
        clientInfoAdapter = new ClientInfoAdapter(this, this);
        lvClient.setAdapter(clientInfoAdapter);

        copyAssets();
        initializeDiscoveryListener();
        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client = new GoogleApiClient.Builder(this).addApi(AppIndex.API).build();
    }

    private class ClientInfoAdapter extends ArrayAdapter<ClientInfo> {
        private Context context;
        private ClientInfoItem.ClientInfoItemListener listener;

        public ClientInfoAdapter(Context context, ClientInfoItem.ClientInfoItemListener listener) {
            super(context, 0);
            this.context = context;
            this.listener = listener;
        }


        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ClientInfo clientInfo = getItem(position);
            final ClientInfoItem clientInfoItem;

            if (convertView != null) {
                clientInfoItem = (ClientInfoItem) convertView;
                clientInfoItem.setClientInfo(clientInfo);
            } else {
                clientInfoItem = new ClientInfoItem(context, clientInfo);
            }
            clientInfoItem.setListener(listener);
            return clientInfoItem;
        }

        public void addClient(ClientInfo clientInfo) {
            if (clientInfo == null)
                return;

            for (int i = 0; i < getCount(); i++) {
                ClientInfo client = getItem(i);
                if (client.getMac().equals(clientInfo.getMac())) {
                    insert(clientInfo, i);
                    remove(client);
                    return;
                }
            }
            add(clientInfo);
        }
    }


    private void copyAssets() {
        AssetManager assetManager = getAssets();
        String[] files = null;
        try {
            files = assetManager.list("");
        } catch (IOException e) {
            Log.e("tag", "Failed to get asset file list.", e);
        }
        if (files != null) for (String filename : files) {
            InputStream in = null;
            OutputStream out = null;
            try {
                File outFile = new File(getFilesDir(), filename);
//                if (outFile.exists() && (outFile.length() == assetManager.openFd(filename).getLength())) {
//                    Log.d("Main", "Exists: " + outFile.getAbsolutePath());
//                    continue;
//                }
                Log.d("Main", "Asset: " + outFile.getAbsolutePath());
                in = assetManager.open(filename);
                out = new FileOutputStream(outFile);
                copyFile(in, out);
                Runtime.getRuntime().exec("chmod 755 " + outFile.getAbsolutePath()).waitFor();
            } catch (Exception e) {
                Log.e("tag", "Failed to copy asset file: " + filename, e);
            } finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
        }
    }

    private void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    private void start() {
        Intent i = new Intent(this, SnapclientService.class);
        i.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        i.putExtra(SnapclientService.EXTRA_HOST, host);
        i.putExtra(SnapclientService.EXTRA_PORT, port);

        startService(i);
    }

    private void stop() {
        stopService(new Intent(this, SnapclientService.class));
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    public void onClick(View view) {
        if (view == buttonStart) {
            start();
        } else if (view == buttonStop) {
            stop();
        } else if (view == buttonScan) {
            Toast.makeText(this, "Scan", Toast.LENGTH_SHORT).show();
            initializeDiscoveryListener();
        } else if (view == button) {
            startTcpClient();
        }
    }

    private void startTcpClient() {
        if ((tcpClient == null) || !tcpClient.isConnected()) {
            if (tcpClient != null)
                tcpClient.stop();
//            Toast.makeText(this, "Connecting", Toast.LENGTH_SHORT).show();
            tcpClient = new TcpClient(this);
            tcpClient.start(host, port + 1);
        } else {
            tcpClient.sendMessage("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"id\": 1}");
        }
    }

    private void stopTcpClient() {
        if ((tcpClient != null) && (tcpClient.isConnected()))
            tcpClient.stop();
        tcpClient = null;
    }


    public void initializeDiscoveryListener() {

        // Instantiate a new DiscoveryListener
        mDiscoveryListener = new NsdManager.DiscoveryListener() {

            private void setStatus(final String text) {
                Log.e(TAG, text);
                MainActivity.this.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        tvHost.setText("Host: " + text);
                    }
                });
            }

            //  Called as soon as service discovery begins.
            @Override
            public void onDiscoveryStarted(String regType) {
                Log.d(TAG, "Service discovery started");
                setStatus("Host: searching for a Snapserver");
            }

            @Override
            public void onServiceFound(NsdServiceInfo service) {
                // A service was found!  Do something with it.
                Log.d(TAG, "Service discovery success" + service);
                mNsdManager.resolveService(service, new NsdManager.ResolveListener() {
                    @Override
                    public void onResolveFailed(NsdServiceInfo nsdServiceInfo, int i) {

                    }

                    @Override
                    public void onServiceResolved(NsdServiceInfo nsdServiceInfo) {
                        Log.d(TAG, "resolved: " + nsdServiceInfo);
                        host = nsdServiceInfo.getHost().getCanonicalHostName();
                        port = nsdServiceInfo.getPort();
                        setStatus(host + ":" + port);
                        startTcpClient();
                        mNsdManager.stopServiceDiscovery(mDiscoveryListener);
                    }
                });
            }

            @Override
            public void onServiceLost(NsdServiceInfo service) {
                // When the network service is no longer available.
                // Internal bookkeeping code goes here.
                Log.e(TAG, "service lost" + service);
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                Log.i(TAG, "Discovery stopped: " + serviceType);
            }

            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                setStatus("Discovery failed: Error code:" + errorCode);
                mNsdManager.stopServiceDiscovery(this);
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                setStatus("Discovery failed: Error code:" + errorCode);
                mNsdManager.stopServiceDiscovery(this);
            }
        };

        mNsdManager = (NsdManager) getSystemService(Context.NSD_SERVICE);
        mNsdManager.discoverServices("_snapcast._tcp.", NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
    }

    @Override
    public void onResume() {
        super.onResume();
        startTcpClient();
    }

    @Override
    public void onStart() {
        super.onStart();
        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client.connect();
        Action viewAction = Action.newAction(
                Action.TYPE_VIEW, // TODO: choose an action type.
                "Main Page", // TODO: Define a title for the content shown.
                // TODO: If you have web page content that matches this app activity's content,
                // make sure this auto-generated web page URL is correct.
                // Otherwise, set the URL to null.
                Uri.parse("http://host/path"),
                // TODO: Make sure this auto-generated app deep link URI is correct.
                Uri.parse("android-app://de.badaix.snapcast/http/host/path")
        );
        AppIndex.AppIndexApi.start(client, viewAction);
    }

    @Override
    public void onStop() {
        stopTcpClient();

        super.onStop();

        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        Action viewAction = Action.newAction(
                Action.TYPE_VIEW, // TODO: choose an action type.
                "Main Page", // TODO: Define a title for the content shown.
                // TODO: If you have web page content that matches this app activity's content,
                // make sure this auto-generated web page URL is correct.
                // Otherwise, set the URL to null.
                Uri.parse("http://host/path"),
                // TODO: Make sure this auto-generated app deep link URI is correct.
                Uri.parse("android-app://de.badaix.snapcast/http/host/path")
        );
        AppIndex.AppIndexApi.end(client, viewAction);
        client.disconnect();
    }

    @Override
    public void onMessageReceived(TcpClient tcpClient, String message) {
        Log.d(TAG, "Msg received: " + message);
        try {
            JSONObject json = new JSONObject(message);
            if (json.has("id")) {
                Log.d(TAG, "ID: " + json.getString("id"));
                if ((json.get("result") instanceof JSONObject) && json.getJSONObject("result").has("clients")) {
                    JSONArray clients = json.getJSONObject("result").getJSONArray("clients");
                    for (int i = 0; i < clients.length(); i++) {
                        final ClientInfo clientInfo = new ClientInfo(clients.getJSONObject(i));
                        Log.d(TAG, "ClientInfo: " + clientInfo);
//                        clientInfoAdapter.add(clientInfo);
                        this.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
//                                if (clientInfo.isConnected())
                                clientInfoAdapter.addClient(clientInfo);
                            }
                        });
                    }
                }
            } else {
                Log.d(TAG, "Notification: " + json.getString("method"));
                if (json.getString("method").equals("Client.OnUpdate") ||
                        json.getString("method").equals("Client.OnConnect") ||
                        json.getString("method").equals("Client.OnDisconnect")) {
                    final ClientInfo clientInfo = new ClientInfo(json.getJSONObject("params").getJSONObject("data"));
                    Log.d(TAG, "ClientInfo: " + clientInfo);
                    Log.d(TAG, "Changed: " + serverInfo.addClient(clientInfo));
//                    clientInfoAdapter.add(clientInfo);
                    this.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
//                            if (clientInfo.isConnected())
                            clientInfoAdapter.addClient(clientInfo);
                        }
                    });
                }
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onConnected(TcpClient tcpClient) {
        Log.d(TAG, "onConnected");
        tcpClient.sendMessage("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"id\": 1}");
    }

    @Override
    public void onDisconnected(TcpClient tcpClient) {
        Log.d(TAG, "onDisconnected");
    }

    @Override
    public void onVolumeChanged(ClientInfoItem clientInfoItem, int percent) {
        ClientInfo client = clientInfoItem.getClientInfo();
        tcpClient.sendMessage("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"" + client.getMac() + "\", \"volume\": " + percent + "}, \"id\": 3}");
    }

    public void onMute(ClientInfoItem clientInfoItem, boolean mute) {
        ClientInfo client = clientInfoItem.getClientInfo();
        tcpClient.sendMessage("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetMute\", \"params\": {\"client\": \"" + client.getMac() + "\", \"mute\": " + mute + "}, \"id\": 3}");
    }

}


