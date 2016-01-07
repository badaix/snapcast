package de.badaix.snapcast;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;


import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.Volume;

public class ClientInfoItem extends LinearLayout implements SeekBar.OnSeekBarChangeListener {

    public interface ClientInfoItemListener {
        void onVolumeChanged(ClientInfoItem clientInfoItem, Volume volume);
    }

    private TextView title;
    private SeekBar volumeSeekBar;
    private ClientInfo clientInfo;
    private ClientInfoItemListener listener = null;

    public ClientInfoItem(Context context, ClientInfo clientInfo) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.client_info, this);
        title = (TextView) findViewById(R.id.title);
        volumeSeekBar = (SeekBar) findViewById(R.id.volumeSeekBar);
        volumeSeekBar.setMax(100);
        setClientInfo(clientInfo);
        volumeSeekBar.setOnSeekBarChangeListener(this);
    }

    public void setClientInfo(final ClientInfo clientInfo) {
        this.clientInfo = clientInfo;
        if (!clientInfo.getName().isEmpty())
            title.setText(clientInfo.getName());
        else
            title.setText(clientInfo.getHost());
        volumeSeekBar.setProgress(clientInfo.getVolume().getPercent());
    }

    public ClientInfo getClientInfo() {
        return clientInfo;
    }

    public void setListener(ClientInfoItemListener listener) {
        this.listener = listener;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser && (listener != null)) {
            Volume volume = new Volume(progress, false);
            clientInfo.setVolume(volume);
            listener.onVolumeChanged(this, volume);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

}
