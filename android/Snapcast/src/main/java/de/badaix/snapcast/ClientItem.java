/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2017  Johannes Pohl
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

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.ServerStatus;
import de.badaix.snapcast.control.json.Volume;

public class ClientItem extends LinearLayout implements SeekBar.OnSeekBarChangeListener, View.OnClickListener, PopupMenu.OnMenuItemClickListener {

    private static final String TAG = "ClientItem";

    private TextView title;
    private SeekBar volumeSeekBar;
    private ImageButton ibMute;
    private ImageButton ibOverflow;
    private Client client;
    private ServerStatus server;
    private ClientItemListener listener = null;

    public ClientItem(Context context, ServerStatus server, Client client) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.client_item, this);
        title = (TextView) findViewById(R.id.title);
        volumeSeekBar = (SeekBar) findViewById(R.id.volumeSeekBar);
        ibMute = (ImageButton) findViewById(R.id.ibMute);
        ibMute.setImageResource(R.drawable.ic_speaker_icon);
        ibMute.setOnClickListener(this);
        ibOverflow = (ImageButton) findViewById(R.id.ibOverflow);
        ibOverflow.setOnClickListener(this);
        volumeSeekBar.setOnSeekBarChangeListener(this);
        this.server = server;
        setClient(client);
    }

    private void update() {
        //Log.d(TAG, "update: " + client.getVisibleName() + ", connected: " + client.isConnected());
        title.setText(client.getVisibleName());
        title.setEnabled(client.isConnected());
        volumeSeekBar.setProgress(client.getConfig().getVolume().getPercent());
        if (client.getConfig().getVolume().isMuted())
            ibMute.setImageResource(R.drawable.ic_mute_icon);
        else
            ibMute.setImageResource(R.drawable.ic_speaker_icon);
    }

    public Client getClient() {
        return client;
    }

    public void setClient(final Client client) {
        this.client = client;
        update();
    }

    public void setListener(ClientItemListener listener) {
        this.listener = listener;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser && (listener != null)) {
            Volume volume = client.getConfig().getVolume();
            volume.setPercent(progress);
            listener.onVolumeChanged(this, volume.getPercent(), volume.isMuted());
        }
    }

    @Override
    public void onClick(View v) {
        if (v == ibMute) {
            Volume volume = client.getConfig().getVolume();
            volume.setMuted(!volume.isMuted());
            update();
            listener.onVolumeChanged(this, volume.getPercent(), volume.isMuted());
        } else if (v == ibOverflow) {
            PopupMenu popup = new PopupMenu(v.getContext(), v);
            popup.getMenu().add(Menu.NONE, R.id.menu_details, 0, R.string.menu_details);
            if (!client.isConnected())
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

    public interface ClientItemListener {
        void onVolumeChanged(ClientItem clientItem, int percent, boolean mute);

        void onDeleteClicked(ClientItem clientItem);

        void onPropertiesClicked(ClientItem clientItem);
    }

}
