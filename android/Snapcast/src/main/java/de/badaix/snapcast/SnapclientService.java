package de.badaix.snapcast;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.app.TaskStackBuilder;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.support.v4.app.NotificationCompat;
import android.util.Log;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;

import static android.os.PowerManager.PARTIAL_WAKE_LOCK;

/**
 * Created by johannes on 01.01.16.
 */

public class SnapclientService extends Service {
    public static final String EXTRA_HOST = "EXTRA_HOST";
    public static final String EXTRA_PORT = "EXTRA_PORT";

    private java.lang.Process process = null;
    private PowerManager.WakeLock wakeLock = null;
    private WifiManager.WifiLock wifiWakeLock = null;
    private Thread reader = null;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Toast.makeText(this, "service starting", Toast.LENGTH_SHORT).show();

        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(this)
                        .setSmallIcon(android.R.drawable.ic_media_play)
                        .setTicker(getText(R.string.ticker_text))
                        .setContentTitle(getText(R.string.notification_title))
                        .setContentText(getText(R.string.notification_text))
                        .setContentInfo(getText(R.string.notification_info));

        Intent resultIntent = new Intent(this, MainActivity.class);

        // The stack builder object will contain an artificial back stack for the
        // started Activity.
        // This ensures that navigating backward from the Activity leads out of
        // your application to the Home screen.
        TaskStackBuilder stackBuilder = TaskStackBuilder.create(this);
        // Adds the back stack for the Intent (but not the Intent itself)
        stackBuilder.addParentStack(MainActivity.class);
        // Adds the Intent that starts the Activity to the top of the stack
        stackBuilder.addNextIntent(resultIntent);
        PendingIntent resultPendingIntent =
                stackBuilder.getPendingIntent(
                        0,
                        PendingIntent.FLAG_UPDATE_CURRENT
                );
        builder.setContentIntent(resultPendingIntent);
        NotificationManager mNotificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        // mId allows you to update the notification later on.
        final Notification notification = builder.build();
//        mNotificationManager.notify(123, notification);
        startForeground(123, notification);


        String host = intent.getStringExtra(EXTRA_HOST);
        int port = intent.getIntExtra(EXTRA_PORT, 1704);
        start(host, port);

        // If we get killed, after returning from here, restart
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        stop();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We don't provide binding, so return null
        return null;
    }

    private void stopService() {
        stop();
        stopForeground(true);
        NotificationManager mNotificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        mNotificationManager.cancel(123);
    }

    private void start(String host, int port) {
        try {
            //https://code.google.com/p/android/issues/detail?id=22763
            stop();
            File binary = new File(getFilesDir(), "snapclient");
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);

            PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
            wakeLock = powerManager.newWakeLock(PARTIAL_WAKE_LOCK, "SnapcastWakeLock");
            wakeLock.acquire();

            WifiManager wm = (WifiManager) getSystemService(WIFI_SERVICE);
            wifiWakeLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "SnapcastWifiWakeLock");
            wifiWakeLock.acquire();

            process = new ProcessBuilder()
                    .command(binary.getAbsolutePath(), "-h", host, "-p", Integer.toString(port))
                    .redirectErrorStream(true)
                    .start();

            Thread reader = new Thread(new Runnable() {
                @Override
                public void run() {
                    BufferedReader bufferedReader = new BufferedReader(
                            new InputStreamReader(process.getInputStream()));
                    String line;
                    try {
                        while ((line = bufferedReader.readLine()) != null) {
                            Log.d("Main", line);
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            });
            reader.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void stop() {
        if (reader != null)
            reader.interrupt();
        if (process != null)
            process.destroy();
        if ((wakeLock != null) && wakeLock.isHeld())
            wakeLock.release();
        if ((wifiWakeLock != null) && wifiWakeLock.isHeld())
            wifiWakeLock.release();
        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DEFAULT);
    }

    // Handler that receives messages from the thread
    private final class ServiceHandler extends Handler {
        public ServiceHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            // Normally we would do some work here, like download a file.
            // For our sample, we just sleep for 5 seconds.
            long endTime = System.currentTimeMillis() + 5 * 1000;
            while (System.currentTimeMillis() < endTime) {
                synchronized (this) {
                    try {
                        wait(endTime - System.currentTimeMillis());
                    } catch (Exception e) {
                    }
                }
            }
            // Stop the service using the startId, so that we don't stop
            // the service in the middle of handling another job
            stopSelf(msg.arg1);
        }
    }

}




