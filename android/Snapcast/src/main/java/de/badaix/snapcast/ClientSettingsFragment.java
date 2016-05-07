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
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.text.TextUtils;
import android.text.format.DateUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.Stream;

/**
 * Created by johannes on 11.01.16.
 */
public class ClientSettingsFragment extends PreferenceFragment {
    private Client client = null;
    private Client clientOriginal = null;
    private EditTextPreference prefName;
    private ListPreference prefStream;
    private EditTextPreference prefLatency;
    private Preference prefMac;
    private Preference prefIp;
    private Preference prefHost;
    private Preference prefOS;
    private Preference prefVersion;
    private Preference prefLastSeen;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle bundle = getArguments();
        try {
            client = new Client(new JSONObject(bundle.getString("client")));
        } catch (JSONException e) {
            e.printStackTrace();
        }
        clientOriginal = new Client(client.toJson());
        final ArrayList<Stream> streams = new ArrayList<>();
        try {
            JSONArray jsonArray = new JSONArray(bundle.getString("streams"));
            for (int i = 0; i < jsonArray.length(); i++)
                streams.add(new Stream(jsonArray.getJSONObject(i)));
        } catch (JSONException e) {
            e.printStackTrace();
        }

        final CharSequence[] streamNames = new CharSequence[streams.size()];
        final CharSequence[] streamIds = new CharSequence[streams.size()];
        for (int i = 0; i < streams.size(); ++i) {
            streamNames[i] = streams.get(i).getName();
            streamIds[i] = streams.get(i).getId();
        }

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.client_preferences);
        prefName = (EditTextPreference) findPreference("pref_client_name");
        prefName.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                prefName.setSummary((String) newValue);
                client.setName((String) newValue);
                return true;
            }
        });
        prefStream = (ListPreference) findPreference("pref_client_stream");
        prefStream.setEntries(streamNames);
        prefStream.setEntryValues(streamIds);
        for (int i = 0; i < streams.size(); ++i) {
            if (streamIds[i].equals(client.getConfig().getStream())) {
                prefStream.setSummary(streamNames[i]);
                prefStream.setValueIndex(i);
                break;
            }
        }
        prefStream.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                for (int i = 0; i < streams.size(); ++i) {
                    if (streamIds[i].equals(newValue)) {
                        prefStream.setSummary(streamNames[i]);
                        client.getConfig().setStream(streamIds[i].toString());
                        prefStream.setValueIndex(i);
                        break;
                    }
                }

                return false;
            }
        });
        prefMac = (Preference) findPreference("pref_client_mac");
        prefIp = (Preference) findPreference("pref_client_ip");
        prefHost = (Preference) findPreference("pref_client_host");
        prefOS = (Preference) findPreference("pref_client_os");
        prefVersion = (Preference) findPreference("pref_client_version");
        prefLastSeen = (Preference) findPreference("pref_client_last_seen");
        prefLatency = (EditTextPreference) findPreference("pref_client_latency");
        prefLatency.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                String latency = (String) newValue;
                if (TextUtils.isEmpty(latency))
                    latency = "0";
                prefLatency.setSummary(latency + "ms");
                client.getConfig().setLatency(Integer.parseInt(latency));
                return true;
            }
        });
        update();
    }

    public Client getClient() {
        return client;
    }

    public Client getOriginalClientInfo() {
        return clientOriginal;
    }

    public void update() {
        if (client == null)
            return;
        prefName.setSummary(client.getConfig().getName());
        prefName.setText(client.getConfig().getName());
        prefMac.setSummary(client.getHost().getMac());
        prefIp.setSummary(client.getHost().getIp());
        prefHost.setSummary(client.getHost().getName());
        prefOS.setSummary(client.getHost().getOs() + "@" + client.getHost().getArch());
        prefVersion.setSummary(client.getSnapclient().getVersion());
        String lastSeen = getText(R.string.online).toString();
        if (!client.isConnected()) {
            long lastSeenTs = Math.min(client.getLastSeen().getSec() * 1000, System.currentTimeMillis());
            lastSeen = DateUtils.getRelativeTimeSpanString(lastSeenTs, System.currentTimeMillis(), DateUtils.SECOND_IN_MILLIS).toString();
        }
        prefLastSeen.setSummary(lastSeen);
        prefLatency.setSummary(client.getConfig().getLatency() + "ms");
        prefLatency.setText(client.getConfig().getLatency() + "");
    }
}
