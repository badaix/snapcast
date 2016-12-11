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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.Group;
import de.badaix.snapcast.control.json.ServerStatus;
import de.badaix.snapcast.control.json.Stream;

/**
 * Created by johannes on 04.12.16.
 */


public class GroupItem extends LinearLayout implements SeekBar.OnSeekBarChangeListener, View.OnClickListener, ClientItem.ClientItemListener {

    private static final String TAG = "GroupItem";

    //    private TextView title;
    private SeekBar volumeSeekBar;
    private ImageButton ibMute;
    private ImageButton ibSettings;
    private LinearLayout llClient;
    private Group group;
    private ServerStatus server;
    private TextView tvStreamState = null;
    private GroupItemListener listener = null;
    private LinearLayout llVolume;

    public GroupItem(Context context, ServerStatus server, Group group) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.group_item, this);
//        title = (TextView) findViewById(R.id.title);
        volumeSeekBar = (SeekBar) findViewById(R.id.volumeSeekBar);
        ibMute = (ImageButton) findViewById(R.id.ibMute);
        ibMute.setImageResource(R.drawable.ic_speaker_icon);
        ibMute.setOnClickListener(this);
        ibSettings = (ImageButton) findViewById(R.id.ibSettings);
        ibSettings.setImageAlpha(138);
        ibSettings.setOnClickListener(this);
        llVolume = (LinearLayout) findViewById(R.id.llVolume);
        llVolume.setVisibility(GONE);
        llClient = (LinearLayout) findViewById(R.id.llClient);
        llClient.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        tvStreamState = (TextView) findViewById(R.id.tvStreamState);
        volumeSeekBar.setOnSeekBarChangeListener(this);
        this.server = server;
        setGroup(group);
    }

    private void update() {
//        title.setText(group.getName());
        llClient.removeAllViews();
        for (Client client : group.getClients()) {
            ClientItem clientItem = new ClientItem(this.getContext(), server, client);
            clientItem.setListener(this);
            llClient.addView(clientItem);
        }

        Stream stream = server.getStream(group.getStreamId());
        if ((tvStreamState == null) || (stream == null))
            return;
        tvStreamState.setText(stream.getName());
/*        String codec = stream.getUri().getQuery().get("codec");
        if (codec.contains(":"))
            codec = codec.split(":")[0];
        tvStreamState.setText(stream.getUri().getQuery().get("sampleformat") + " - " + codec + " - " + stream.getStatus().toString());

        title.setEnabled(group.isConnected());
        volumeSeekBar.setProgress(group.getConfig().getVolume().getPercent());
        if (client.getConfig().getVolume().isMuted())
            ibMute.setImageResource(R.drawable.ic_mute_icon);
        else
            ibMute.setImageResource(R.drawable.ic_speaker_icon);
*/
    }

    public Group getGroup() {
        return group;
    }

    public void setGroup(final Group group) {
        this.group = group;
        update();
    }

    public void setListener(GroupItemListener listener) {
        this.listener = listener;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
/*        if (fromUser && (listener != null)) {
            Volume volume = new Volume(progress, false);
            client.setVolume(volume);
            listener.onVolumeChanged(this, volume.getPercent());
        }
*/
    }

    @Override
    public void onClick(View v) {
/* TODO: group        if (v == ibMute) {
            Volume volume = client.getConfig().getVolume();
            volume.setMuted(!volume.isMuted());
            update();
            listener.onMute(this, volume.isMuted());
        } else
*/
        if (v == ibSettings) {
            listener.onPropertiesClicked(this);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onVolumeChanged(ClientItem clientItem, int percent) {
        if (listener != null)
            listener.onVolumeChanged(this, clientItem, percent);
    }

    @Override
    public void onMute(ClientItem clientItem, boolean mute) {
        if (listener != null)
            listener.onMute(this, clientItem, mute);
    }

    @Override
    public void onDeleteClicked(ClientItem clientItem) {
        if (listener != null)
            listener.onDeleteClicked(this, clientItem);
    }

    @Override
    public void onPropertiesClicked(ClientItem clientItem) {
        if (listener != null)
            listener.onClientPropertiesClicked(this, clientItem);
    }


    public interface GroupItemListener {
        void onVolumeChanged(GroupItem group, ClientItem clientItem, int percent);

        void onMute(GroupItem group, ClientItem clientItem, boolean mute);

        void onDeleteClicked(GroupItem group, ClientItem clientItem);

        void onClientPropertiesClicked(GroupItem group, ClientItem clientItem);

        void onPropertiesClicked(GroupItem group);

        void onStreamClicked(GroupItem group, Stream stream);
    }

}
