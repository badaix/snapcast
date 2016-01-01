package de.badaix.snapcast;

import android.app.Activity;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.*;
import android.provider.Settings;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBar;
import android.support.v7.app.ActionBarActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.appindexing.Action;
import com.google.android.gms.appindexing.AppIndex;
import com.google.android.gms.common.api.GoogleApiClient;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.lang.Process;
import java.lang.reflect.Array;
import java.net.InetAddress;

import static android.os.PowerManager.PARTIAL_WAKE_LOCK;

public class MainActivity extends ActionBarActivity
        implements NavigationDrawerFragment.NavigationDrawerCallbacks {

    /**
     * Fragment managing the behaviors, interactions and presentation of the navigation drawer.
     */
    private NavigationDrawerFragment mNavigationDrawerFragment;

    /**
     * Used to store the last screen title. For use in {@link #restoreActionBar()}.
     */
    private CharSequence mTitle;

    private static final String TAG = "Main";
    /**
     * ATTENTION: This was auto-generated to implement the App Indexing API.
     * See https://g.co/AppIndexing/AndroidStudio for more information.
     */
    private GoogleApiClient client;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mNavigationDrawerFragment = (NavigationDrawerFragment)
                getSupportFragmentManager().findFragmentById(R.id.navigation_drawer);
        mTitle = getTitle();

        // Set up the drawer.
        mNavigationDrawerFragment.setUp(
                R.id.navigation_drawer,
                (DrawerLayout) findViewById(R.id.drawer_layout));

        copyAssets();
        // ATTENTION: This was auto-generated to implement the App Indexing API.
        // See https://g.co/AppIndexing/AndroidStudio for more information.
        client = new GoogleApiClient.Builder(this).addApi(AppIndex.API).build();
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


    @Override
    public void onNavigationDrawerItemSelected(int position) {
        // update the main content by replacing fragments
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction()
                .replace(R.id.container, PlaceholderFragment.newInstance(position + 1))
                .commit();
    }

    public void onSectionAttached(int number) {
        switch (number) {
            case 1:
                mTitle = getString(R.string.title_section1);
                break;
            case 2:
                mTitle = getString(R.string.title_section2);
                break;
            case 3:
                mTitle = getString(R.string.title_section3);
                break;
        }
    }

    public void restoreActionBar() {
        ActionBar actionBar = getSupportActionBar();
        actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_STANDARD);
        actionBar.setDisplayShowTitleEnabled(true);
        actionBar.setTitle(mTitle);
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


    /**
     * A placeholder fragment containing a simple view.
     */
    public static class PlaceholderFragment extends Fragment implements View.OnClickListener {
        /**
         * The fragment argument representing the section number for this
         * fragment.
         */
        private static final String ARG_SECTION_NUMBER = "section_number";
        private Button buttonScan;
        private Button buttonStart;
        private Button buttonStop;
        private TextView tvHost;
        private CheckBox cbScreenWakelock;
        private NsdManager.DiscoveryListener mDiscoveryListener;
        private NsdManager mNsdManager = null;
        private String host = "";
        private int port = 1704;


        public PlaceholderFragment() {
        }

        /**
         * Returns a new instance of this fragment for the given section
         * number.
         */
        public static PlaceholderFragment newInstance(int sectionNumber) {
            PlaceholderFragment fragment = new PlaceholderFragment();
            Bundle args = new Bundle();
            args.putInt(ARG_SECTION_NUMBER, sectionNumber);
            fragment.setArguments(args);
            return fragment;
        }

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                                 Bundle savedInstanceState) {
            View rootView = inflater.inflate(R.layout.fragment_main, container, false);
            buttonScan = (Button) rootView.findViewById(R.id.buttonScan);
            buttonStart = (Button) rootView.findViewById(R.id.buttonStart);
            buttonStop = (Button) rootView.findViewById(R.id.buttonStop);
            tvHost = (TextView) rootView.findViewById(R.id.tvHost);
            cbScreenWakelock = (CheckBox) rootView.findViewById(R.id.cbScreenWakelock);
            cbScreenWakelock.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                    if (b)
                        getActivity().getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    else
                        getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            });

            buttonScan.setOnClickListener(this);
            buttonStart.setOnClickListener(this);
            buttonStop.setOnClickListener(this);
            initializeDiscoveryListener();
            return rootView;
        }

        @Override
        public void onAttach(Activity activity) {
            super.onAttach(activity);
            ((MainActivity) activity).onSectionAttached(
                    getArguments().getInt(ARG_SECTION_NUMBER));
        }


        private void start() {
            Intent i = new Intent(this.getActivity(), SnapclientService.class);

            i.putExtra(SnapclientService.EXTRA_HOST, host);
            i.putExtra(SnapclientService.EXTRA_PORT, port);

            getActivity().startService(i);

        }

        private void stop() {
            getActivity().stopService(new Intent(this.getActivity(), SnapclientService.class));
            getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }

        @Override
        public void onClick(View view) {
            if (view == buttonStart) {
                Toast.makeText(this.getActivity(), "Start", Toast.LENGTH_SHORT).show();
                start();
            } else if (view == buttonStop) {
                Toast.makeText(this.getActivity(), "Stop", Toast.LENGTH_SHORT).show();
                stop();
            } else if (view == buttonScan) {
                Toast.makeText(this.getActivity(), "Scan", Toast.LENGTH_SHORT).show();
                initializeDiscoveryListener();
            }
        }

        public void initializeDiscoveryListener() {

            // Instantiate a new DiscoveryListener
            mDiscoveryListener = new NsdManager.DiscoveryListener() {

                private void setStatus(final String text) {
                    Log.e(TAG, text);
                    tvHost.post(new Runnable() {
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

            mNsdManager = (NsdManager) this.getActivity().getSystemService(Context.NSD_SERVICE);
            mNsdManager.discoverServices("_snapcast._tcp.", NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
        }
    }

}
