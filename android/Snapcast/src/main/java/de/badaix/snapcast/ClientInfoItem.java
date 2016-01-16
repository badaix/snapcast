package de.badaix.snapcast;

import android.content.Context;
import android.support.v7.widget.PopupMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.Volume;

public class ClientInfoItem extends LinearLayout implements SeekBar.OnSeekBarChangeListener, View.OnClickListener, PopupMenu.OnMenuItemClickListener {

    private TextView title;
    private SeekBar volumeSeekBar;
    private ImageButton ibMute;
    private ImageButton ibOverflow;
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
        ibOverflow = (ImageButton) findViewById(R.id.ibOverflow);
        ibOverflow.setOnClickListener(this);
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

    public ClientInfo getClientInfo() {
        return clientInfo;
    }

    public void setClientInfo(final ClientInfo clientInfo) {
        this.clientInfo = clientInfo;
        update();
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
        if (v == ibMute) {
            Volume volume = clientInfo.getVolume();
            volume.setMuted(!volume.isMuted());
            update();
            listener.onMute(this, volume.isMuted());
        } else if (v == ibOverflow) {
            PopupMenu popup = new PopupMenu(v.getContext(), v);
            popup.getMenu().add(Menu.NONE, R.id.menu_details, 0, R.string.menu_details);
            if (!clientInfo.isConnected())
                popup.getMenu().add(Menu.NONE, R.id.menu_delete, 1, R.string.menu_delete);
            popup.setOnMenuItemClickListener(this);
            popup.show();
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_details:
                listener.onPropertiesClicked(this);
                return true;
            case R.id.menu_delete:
                listener.onDeleteClicked(this);
                return true;
            default:
                return false;
        }
    }

    public interface ClientInfoItemListener {
        void onVolumeChanged(ClientInfoItem clientInfoItem, int percent);

        void onMute(ClientInfoItem clientInfoItem, boolean mute);

        void onDeleteClicked(ClientInfoItem clientInfoItem);

        void onPropertiesClicked(ClientInfoItem clientInfoItem);
    }

}
