package de.badaix.snapcast;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import de.badaix.snapcast.utils.Settings;

/**
 * Created by johannes on 05.05.16.
 */
public class BootCompleteReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if ("android.intent.action.BOOT_COMPLETED".equals(intent.getAction())) {
            if (Settings.getInstance(context).isAutostart()) {
                String host = Settings.getInstance(context).getHost();
                int port = Settings.getInstance(context).getStreamPort();
                if (TextUtils.isEmpty(host))
                    return;

                Intent i = new Intent(context, SnapclientService.class);
                i.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                i.putExtra(SnapclientService.EXTRA_HOST, host);
                i.putExtra(SnapclientService.EXTRA_PORT, port);
                i.setAction(SnapclientService.ACTION_START);

                context.startService(i);
            }
        }
    }
}


