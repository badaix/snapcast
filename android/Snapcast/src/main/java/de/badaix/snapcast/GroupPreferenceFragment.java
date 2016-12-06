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

import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.Group;
import de.badaix.snapcast.control.json.ServerStatus;
import de.badaix.snapcast.control.json.Stream;

/**
 * Created by johannes on 06.12.16.
 */

public class GroupPreferenceFragment extends PreferenceFragment {
    private ListPreference prefStreams;
    private Group group = null;
    private ServerStatus serverStatus = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.group_preferences);
        PreferenceScreen screen = this.getPreferenceScreen();

        prefStreams = (ListPreference) findPreference("pref_stream");

        Bundle bundle = getArguments();
        try {
            group = new Group(new JSONObject(bundle.getString("group")));
            serverStatus = new ServerStatus(new JSONObject(bundle.getString("serverStatus")));
        } catch (JSONException e) {
            e.printStackTrace();
        }

        final ArrayList<Stream> streams = serverStatus.getStreams();
        final CharSequence[] streamNames = new CharSequence[streams.size()];
        final CharSequence[] streamIds = new CharSequence[streams.size()];
        for (int i = 0; i < streams.size(); ++i) {
            streamNames[i] = streams.get(i).getName();
            streamIds[i] = streams.get(i).getId();
        }

        prefStreams.setEntries(streamNames);
        prefStreams.setEntryValues(streamIds);

        for (int i = 0; i < streams.size(); ++i) {
            if (streamIds[i].equals(group.getStreamId())) {
                prefStreams.setTitle(streamNames[i]);
                prefStreams.setValueIndex(i);
                break;
            }
        }

        prefStreams.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                for (int i = 0; i < streams.size(); ++i) {
                    if (streamIds[i].equals(newValue)) {
                        prefStreams.setTitle(streamNames[i]);
//                        client.getConfig().setStream(streamIds[i].toString());
                        prefStreams.setValueIndex(i);
                        break;
                    }
                }

                return false;
            }
        });


        PreferenceCategory prefStream = (PreferenceCategory) findPreference("pref_cat_clients");

        for (Group g : serverStatus.getGroups()) {
            for (Client client : g.getClients()) {
                CheckBoxPreference checkBoxPref = new CheckBoxPreference(screen.getContext());
                checkBoxPref.setKey(client.getId());
                checkBoxPref.setTitle(client.getVisibleName());
                checkBoxPref.setChecked(g.getId().equals(group.getId()));
                prefStream.addPreference(checkBoxPref);
            }
        }
    }
}