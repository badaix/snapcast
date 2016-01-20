package de.badaix.snapcast;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.net.Uri;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.support.design.widget.Snackbar;
import android.support.design.widget.Snackbar.Callback;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.widget.TextView;

import com.google.android.gms.appindexing.Action;
import com.google.android.gms.appindexing.AppIndex;
import com.google.android.gms.common.api.GoogleApiClient;

import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.RemoteControl;
import de.badaix.snapcast.control.ServerInfo;
import de.badaix.snapcast.utils.NsdHelper;
import de.badaix.snapcast.utils.Setup;

public class MainActivity extends AppCompatActivity implements ClientInfoItem.ClientInfoItemListener, RemoteControl.RemoteControlListener, SnapclientService.SnapclientListener, NsdHelper.NsdHelperListener {

    private static final String TAG = "Main";
    boolean bound = false;
    private MenuItem miStartStop = null;
    private String host = "";
    private int port = 1704;
    private RemoteControl remoteControl = null;
    private ClientInfoAdapter clientInfoAdapter;
    private SnapclientService snapclientService;

    /**
     * ATTENTION: This was auto-generated to implement the App Indexing API.
     * See https://g.co/AppIndexing/AndroidStudio for more information.
     */
    private GoogleApiClient client;

    /**
     * Defines callbacks for service binding, passed to bindService()
     */
    private ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName className,
                                       IBinder service) {
            // We've bound to LocalService, cast the IBinder and get LocalService instance
            SnapclientService.LocalBinder binder = (SnapclientService.LocalBinder) service;
            snapclientService = binder.getService();
            snapclientService.setListener(MainActivity.this);
            bound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            bound = false;
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView tvInfo = (TextView) findViewById(R.id.tvInfo);
        CheckBox cbScreenWakelock = (CheckBox) findViewById(R.id.cbScreenWakelock);
        cbScreenWakelock.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                if (b)
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });

        AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            String rate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            String size = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            tvInfo.setText("Sample rate: " + rate + ", buffer size: " + size);
        }

        clientInfoAdapter = new ClientInfoAdapter(this, this);
        ListView lvClient = (ListView) findViewById(R.id.lvClient);
        lvClient.setAdapter(clientInfoAdapter);
        getSupportActionBar().setSubtitle("Host: no Snapserver found");

        new Thread(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "copying snapclient");
                Setup.copyAssets(MainActivity.this, new String[]{"snapclient"});
                Log.d(TAG, "done copying snapclient");
            }
        }).start();
//        initializeDiscoveryListener();
        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client = new GoogleApiClient.Builder(this).addApi(AppIndex.API).build();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_snapcast, menu);
        miStartStop = menu.findItem(R.id.action_play_stop);
        updateStartStopMenuItem();
        SharedPreferences settings = getSharedPreferences("settings", 0);
        boolean isChecked = settings.getBoolean("hide_offline", false);
        MenuItem menuItem = menu.findItem(R.id.action_hide_offline);
        menuItem.setChecked(isChecked);
        clientInfoAdapter.setHideOffline(isChecked);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_scan) {
//            NsdHelper.getInstance(this).startListening("*._tcp.", "SnapCast", this);
            NsdHelper.getInstance(this).startListening("_snapcast._tcp.", "SnapCast", this);
            return true;
        } else if (id == R.id.action_play_stop) {
            if (bound && snapclientService.isRunning()) {
                stopSnapclient();
            } else {
                startSnapclient();
            }
            return true;
        } else if (id == R.id.action_hide_offline) {
            item.setChecked(!item.isChecked());
            SharedPreferences settings = getSharedPreferences("settings", 0);
            SharedPreferences.Editor editor = settings.edit();
            editor.putBoolean("hide_offline", item.isChecked());
            editor.apply();
            clientInfoAdapter.setHideOffline(item.isChecked());
            return true;
        } else if (id == R.id.action_refresh) {
            if ((remoteControl != null) && remoteControl.isConnected())
                remoteControl.getServerStatus();
        } else if (id == R.id.action_about) {
            Intent intent = new Intent(this, AboutActivity.class);
            startActivity(intent);
        }

        return super.onOptionsItemSelected(item);
    }

    private void updateStartStopMenuItem() {
        if (bound && snapclientService.isRunning()) {
            miStartStop.setIcon(R.drawable.ic_media_stop);
        } else {
            miStartStop.setIcon(R.drawable.ic_media_play);
        }
    }

    private void startSnapclient() {
        Intent i = new Intent(this, SnapclientService.class);
        i.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        i.putExtra(SnapclientService.EXTRA_HOST, host);
        i.putExtra(SnapclientService.EXTRA_PORT, port);
        i.setAction(SnapclientService.ACTION_START);

        startService(i);
    }

    private void stopSnapclient() {
        if (bound)
            snapclientService.stopPlayer();
//        stopService(new Intent(this, SnapclientService.class));
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }


    private void startRemoteControl() {
        if (remoteControl == null)
            remoteControl = new RemoteControl(this);
        if (!host.isEmpty())
            remoteControl.connect(host, port + 1);
    }

    private void stopRemoteControl() {
        if ((remoteControl != null) && (remoteControl.isConnected()))
            remoteControl.disconnect();
        remoteControl = null;
    }


    @Override
    public void onResume() {
        super.onResume();
        startRemoteControl();
    }

    @Override
    public void onStart() {
        super.onStart();

        NsdHelper.getInstance(this).startListening("_snapcast._tcp.", "SnapCast", this);

        Intent intent = new Intent(this, SnapclientService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

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
    public void onDestroy() {
        stopRemoteControl();
        super.onDestroy();
    }

    @Override
    public void onStop() {
        super.onStop();

        NsdHelper.getInstance(this).stopListening();
// Unbind from the service
        if (bound) {
            unbindService(mConnection);
            bound = false;
        }

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
    public void onPlayerStart() {
        updateStartStopMenuItem();
    }

    @Override
    public void onPlayerStop() {
        updateStartStopMenuItem();
    }

    @Override
    public void onLog(String log) {
        Log.d(TAG, log);
    }


    @Override
    public void onVolumeChanged(ClientInfoItem clientInfoItem, int percent) {
        remoteControl.setVolume(clientInfoItem.getClientInfo(), percent);
    }

    @Override
    public void onMute(ClientInfoItem clientInfoItem, boolean mute) {
        remoteControl.setMute(clientInfoItem.getClientInfo(), mute);
    }

    @Override
    public void onDeleteClicked(final ClientInfoItem clientInfoItem) {
        final ClientInfo clientInfo = clientInfoItem.getClientInfo();
        clientInfo.setDeleted(true);
        clientInfoAdapter.update();
        Snackbar mySnackbar = Snackbar.make(findViewById(R.id.myCoordinatorLayout),
                getString(R.string.client_deleted, clientInfo.getVisibleName()),
                Snackbar.LENGTH_SHORT);
        mySnackbar.setAction(R.string.undo_string, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                clientInfo.setDeleted(false);
                clientInfoAdapter.update();
            }
        });
        mySnackbar.setCallback(new Callback() {
            @Override
            public void onDismissed(Snackbar snackbar, int event) {
                super.onDismissed(snackbar, event);
                if (event != DISMISS_EVENT_ACTION) {
                    remoteControl.delete(clientInfo);
                    clientInfoAdapter.removeClient(clientInfo);
                }
            }
        });
        mySnackbar.show();
    }

    @Override
    public void onPropertiesClicked(ClientInfoItem clientInfoItem) {
        Intent intent = new Intent(this, ClientSettingsActivity.class);
        intent.putExtra("clientInfo", clientInfoItem.getClientInfo());
        startActivityForResult(intent, 1);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == RESULT_CANCELED) {
            return;
        }
        if (requestCode == 1) {
            ClientInfo clientInfo = data.getParcelableExtra("clientInfo");
            remoteControl.setName(clientInfo, clientInfo.getName());
            clientInfoAdapter.updateClient(clientInfo);
        }
    }

    @Override
    public void onConnected(RemoteControl remoteControl) {
        remoteControl.getServerStatus();
    }

    @Override
    public void onDisconnected(RemoteControl remoteControl) {
        Log.d(TAG, "onDisconnected");
    }

    @Override
    public void onClientEvent(RemoteControl remoteControl, ClientInfo clientInfo, RemoteControl.ClientEvent event) {
        Log.d(TAG, "onClientEvent: " + event.toString());
        if (event == RemoteControl.ClientEvent.deleted)
            clientInfoAdapter.removeClient(clientInfo);
        else
            clientInfoAdapter.updateClient(clientInfo);

    }

    @Override
    public void onServerInfo(RemoteControl remoteControl, ServerInfo serverInfo) {
        clientInfoAdapter.updateServer(serverInfo);
    }

    private void setActionbarSubtitle(final String subtitle) {
        MainActivity.this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ActionBar actionBar = getSupportActionBar();
                if (actionBar != null)
                    actionBar.setSubtitle(subtitle);
            }
        });
    }

    @Override
    public void onResolved(NsdHelper nsdHelper, NsdServiceInfo serviceInfo) {
        Log.d(TAG, "resolved: " + serviceInfo);
        host = serviceInfo.getHost().getCanonicalHostName();
        port = serviceInfo.getPort();
        setActionbarSubtitle(host + ":" + port);
        startRemoteControl();
        NsdHelper.getInstance(this).stopListening();
    }

    private class ClientInfoAdapter extends ArrayAdapter<ClientInfo> {
        private Context context;
        private ClientInfoItem.ClientInfoItemListener listener;
        private boolean hideOffline = false;
        private ServerInfo serverInfo = new ServerInfo();

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

        public void removeClient(final ClientInfo clientInfo) {
            if ((clientInfo != null) && serverInfo.removeClient(clientInfo)) {
                Log.d(TAG, "removeClient 1: " + clientInfo.getMac());
                update();
            }
        }

        public void updateClient(final ClientInfo clientInfo) {
            Log.d(TAG, "updateClient: " + clientInfo.getHost() + " " + clientInfo.isDeleted());
            if (serverInfo.updateClient(clientInfo)) {
                update();
            }
        }

        public void updateServer(final ServerInfo serverInfo) {
            if (serverInfo != null) {
                ClientInfoAdapter.this.serverInfo = serverInfo;
                update();
            }
        }


        public void update() {

            MainActivity.this.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    clear();
                    for (ClientInfo clientInfo : ClientInfoAdapter.this.serverInfo.getClientInfos()) {
                        if ((clientInfo != null) && (!hideOffline || clientInfo.isConnected()) && !clientInfo.isDeleted())
                            add(clientInfo);
                    }
                    notifyDataSetChanged();
                }
            });
        }

        public boolean isHideOffline() {
            return hideOffline;
        }

        public void setHideOffline(boolean hideOffline) {
            if (this.hideOffline == hideOffline)
                return;
            this.hideOffline = hideOffline;
            update();
        }
    }
}


