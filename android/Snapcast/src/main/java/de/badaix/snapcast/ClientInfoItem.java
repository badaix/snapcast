package de.badaix.snapcast;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;


import de.badaix.snapcast.control.ClientInfo;

public class ClientInfoItem extends LinearLayout {

    public static interface OnAppItemChangedListener {
        abstract void onChanged(ClientInfo clientInfo, boolean enabled);
    }

    private TextView title;
    private TextView summary;
    private SeekBar volumeSeekBar;
    private ClientInfo clientInfo;

    public ClientInfoItem(Context context, ClientInfo clientInfo) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.client_info, this);
        title = (TextView) findViewById(R.id.title);
        summary = (TextView) findViewById(R.id.summary);
        volumeSeekBar = (SeekBar) findViewById(R.id.volumeSeekBar);
        volumeSeekBar.setMax(100);
        setClientInfo(clientInfo);
    }

    public void setClientInfo(final ClientInfo clientInfo) {
        this.clientInfo = clientInfo;
        if (!clientInfo.getName().isEmpty())
            title.setText(clientInfo.getName());
        else
            title.setText(clientInfo.getHost());
        summary.setText(clientInfo.getMac());
        volumeSeekBar.setProgress(clientInfo.getVolume().getPercent());
    }

    public ClientInfo getClientInfo() {
        return clientInfo;
    }

}
