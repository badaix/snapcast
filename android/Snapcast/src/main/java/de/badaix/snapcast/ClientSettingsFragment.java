package de.badaix.snapcast;

import android.os.Bundle;
import android.preference.EditTextPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.text.format.DateUtils;
import android.util.Log;

import de.badaix.snapcast.control.ClientInfo;

/**
 * Created by johannes on 11.01.16.
 */
public class ClientSettingsFragment extends PreferenceFragment {
    private ClientInfo clientInfo = null;
    private EditTextPreference prefName;
    private Preference prefMac;
    private Preference prefIp;
    private Preference prefHost;
    private Preference prefVersion;
    private Preference prefLastSeen;
    private Preference prefLatency;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        clientInfo = (ClientInfo)getArguments().getParcelable("clientInfo");

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.client_preferences);
        prefName = (EditTextPreference) findPreference("pref_client_name");
        prefName.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                prefName.setSummary((String)newValue);
                clientInfo.setName((String)newValue);
                return true;
            }
        });
        prefMac = (Preference) findPreference("pref_client_mac");
        prefIp = (Preference) findPreference("pref_client_ip");
        prefHost = (Preference) findPreference("pref_client_host");
        prefVersion = (Preference) findPreference("pref_client_version");
        prefLastSeen = (Preference) findPreference("pref_client_last_seen");
        prefLatency = (Preference) findPreference("pref_client_latency");
        update();
    }

    public ClientInfo getClientInfo() {
        return clientInfo;
    }

    public void update() {
        if (clientInfo == null)
            return;
        prefName.setSummary(clientInfo.getName());
        prefName.setText(clientInfo.getName());
        prefMac.setSummary(clientInfo.getMac());
        prefIp.setSummary(clientInfo.getIp());
        prefHost.setSummary(clientInfo.getHost());
        prefVersion.setSummary(clientInfo.getVersion());
        String lastSeen = getText(R.string.online).toString();
        if (!clientInfo.isConnected()) {
            long lastSeenTs = Math.min(clientInfo.getLastSeen().getSec() * 1000, System.currentTimeMillis());
            lastSeen = DateUtils.getRelativeTimeSpanString(lastSeenTs, System.currentTimeMillis(), DateUtils.SECOND_IN_MILLIS).toString();
        }
        prefLastSeen.setSummary(lastSeen);
        prefLatency.setSummary(clientInfo.getLatency() + "ms");
    }
}
