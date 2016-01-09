package de.badaix.snapcast;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;


import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.Volume;

public class ClientInfoItem extends LinearLayout implements SeekBar.OnSeekBarChangeListener, View.OnClickListener {

    public interface ClientInfoItemListener {
        void onVolumeChanged(ClientInfoItem clientInfoItem, int percent);
        void onMute(ClientInfoItem clientInfoItem, boolean mute);
    }

    private TextView title;
    private SeekBar volumeSeekBar;
    private ImageButton ibMute;
    private ClientInfo clientInfo;
    private ClientInfoItemListener listener = null;

    public ClientInfoItem(Context context, ClientInfo clientInfo) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.client_info, this);
        title = (TextView) findViewById(R.id.title);
        volumeSeekBar = (SeekBar) findViewById(R.id.volumeSeekBar);
        ibMute = (ImageButton) findViewById(R.id.ibMute);
        ibMute.setImageResource(R.drawable.ic_speaker_icon);
        ibMute.setOnClickListener(this);
        volumeSeekBar.setMax(100);
        setClientInfo(clientInfo);
        volumeSeekBar.setOnSeekBarChangeListener(this);
    }

    private void update() {
        if (!clientInfo.getName().isEmpty())
            title.setText(clientInfo.getName());
        else
            title.setText(clientInfo.getHost());
        title.setEnabled(clientInfo.isConnected());
        volumeSeekBar.setProgress(clientInfo.getVolume().getPercent());
        if (clientInfo.getVolume().isMuted())
            ibMute.setImageResource(R.drawable.ic_mute_icon);
        else
            ibMute.setImageResource(R.drawable.ic_speaker_icon);
    }

    public void setClientInfo(final ClientInfo clientInfo) {
        this.clientInfo = clientInfo;
        update();
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
            listener.onVolumeChanged(this, volume.getPercent());
        }
    }

    @Override
    public void onClick(View v) {
        Volume volume = clientInfo.getVolume();
        volume.setMuted(!volume.isMuted());
        update();
        listener.onMute(this, volume.isMuted());
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

}
