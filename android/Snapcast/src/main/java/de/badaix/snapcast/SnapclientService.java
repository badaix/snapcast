/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2016  Johannes Pohl
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package de.badaix.snapcast;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.TaskStackBuilder;

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
    public static final String ACTION_START = "ACTION_START";
    public static final String ACTION_STOP = "ACTION_STOP";
    private final IBinder mBinder = new LocalBinder();
    private java.lang.Process process = null;
    private PowerManager.WakeLock wakeLock = null;
    private WifiManager.WifiLock wifiWakeLock = null;
    private Thread reader = null;
    private boolean running = false;
    private SnapclientListener listener = null;
    private boolean logReceived;

    public boolean isRunning() {
        return running;
    }

    public void setListener(SnapclientListener listener) {
        this.listener = listener;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null)
            return START_NOT_STICKY;

        if (intent.getAction() == ACTION_STOP) {
            stopService();
            return START_NOT_STICKY;
        } else if (intent.getAction() == ACTION_START) {
            String host = intent.getStringExtra(EXTRA_HOST);
            int port = intent.getIntExtra(EXTRA_PORT, 1704);

            Intent stopIntent = new Intent(this, SnapclientService.class);
            stopIntent.setAction(ACTION_STOP);
            PendingIntent piStop = PendingIntent.getService(this, 0, stopIntent, 0);

            NotificationCompat.Builder builder =
                    new NotificationCompat.Builder(this)
                            .setSmallIcon(R.drawable.ic_media_play)
                            .setTicker(getText(R.string.ticker_text))
                            .setContentTitle(getText(R.string.notification_title))
                            .setContentText(getText(R.string.notification_text))
                            .setContentInfo(host)
                            .setStyle(new NotificationCompat.BigTextStyle().bigText(getText(R.string.notification_text)))
                            .addAction(R.drawable.ic_media_stop, getString(R.string.stop), piStop);

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

            start(host, port);
            return START_STICKY;
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        stop();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    public void stopPlayer() {
        stopService();
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
            if (running)
                return;
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
                            log(line);
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            });
            logReceived = false;
            reader.start();

            //TODO: wait for started message on stdout
/*            long now = System.currentTimeMillis();
            while (!logReceived) {
                if (System.currentTimeMillis() > now + 1000)
                    throw new Exception("start timeout");
                Thread.sleep(100, 0);
            }
*/
        } catch (Exception e) {
            e.printStackTrace();
            if (listener != null)
                listener.onError(this, e.getMessage(), e);
            stop();
        }
    }

    private void log(String msg) {
        if (!logReceived) {
            logReceived = true;
            running = true;
            if (listener != null)
                listener.onPlayerStart(this);
        }
        if (listener != null) {
            int idxBracketOpen = msg.indexOf('[');
            int idxBracketClose = msg.indexOf(']', idxBracketOpen);
            if ((idxBracketOpen > 0) && (idxBracketClose > 0)) {
                listener.onLog(SnapclientService.this, msg.substring(0, idxBracketOpen - 1), msg.substring(idxBracketOpen + 1, idxBracketClose), msg.substring(idxBracketClose + 2));
            }
        }
    }

    private void stop() {
        try {
            if (reader != null)
                reader.interrupt();
            if (process != null)
                process.destroy();
            if ((wakeLock != null) && wakeLock.isHeld())
                wakeLock.release();
            if ((wifiWakeLock != null) && wifiWakeLock.isHeld())
                wifiWakeLock.release();
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DEFAULT);
            running = false;
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (listener != null)
            listener.onPlayerStop(this);
    }

    public interface SnapclientListener {
        void onPlayerStart(SnapclientService snapclientService);

        void onPlayerStop(SnapclientService snapclientService);

        void onLog(SnapclientService snapclientService, String timestamp, String logClass, String msg);

        void onError(SnapclientService snapclientService, String msg, Exception exception);
    }

    /**
     * Class used for the client Binder.  Because we know this service always
     * runs in the same process as its clients, we don't need to deal with IPC.
     */
    public class LocalBinder extends Binder {
        SnapclientService getService() {
            // Return this instance of LocalService so clients can call public methods
            return SnapclientService.this;
        }
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




