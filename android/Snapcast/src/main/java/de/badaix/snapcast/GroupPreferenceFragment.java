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

/**
 * Created by johannes on 06.12.16.
 */

public class GroupPreferenceFragment extends PreferenceFragment {
    private ListPreference prefStreams;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.group_preferences);
        PreferenceScreen screen = this.getPreferenceScreen();

        prefStreams = (ListPreference) findPreference("pref_stream");
        final CharSequence[] entries = { "English", "French" };
        final CharSequence[] entryValues = {"1" , "2"};
        prefStreams.setEntries(entries);
        prefStreams.setDefaultValue("1");
        prefStreams.setEntryValues(entryValues);
        prefStreams.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                preference.setSummary(newValue.toString());
                prefStreams.setTitle(entries[prefStreams.findIndexOfValue(newValue.toString())]);
                return true;
            }
        });

        PreferenceCategory prefStream = (PreferenceCategory) findPreference("pref_cat_clients");

        for (int i=0; i<3; ++i) {
            CheckBoxPreference checkBoxPref = new CheckBoxPreference(screen.getContext());
            checkBoxPref.setKey("key" + i);
            checkBoxPref.setTitle("Title " + i);
            checkBoxPref.setChecked(true);
            prefStream.addPreference(checkBoxPref);
        }
    }
}