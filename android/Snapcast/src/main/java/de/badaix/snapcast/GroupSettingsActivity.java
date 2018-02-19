/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2018  Johannes Pohl
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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.MenuItem;

/**
 * Created by johannes on 06.12.16.
 */

public class GroupSettingsActivity extends AppCompatActivity {

    private GroupSettingsFragment groupSettingsFragment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        groupSettingsFragment = new GroupSettingsFragment();
        groupSettingsFragment.setArguments(getIntent().getExtras());
        // Display the fragment as the main content.
        getFragmentManager().beginTransaction()
                .replace(android.R.id.content, groupSettingsFragment).commit();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return false;
    }

    @Override
    public void onBackPressed() {
        Intent intent = new Intent();
        intent.putExtra("group", groupSettingsFragment.getGroup().getId());
        if (groupSettingsFragment.didStreamChange())
            intent.putExtra("stream", groupSettingsFragment.getStream());
        if (groupSettingsFragment.didClientsChange())
            intent.putStringArrayListExtra("clients", groupSettingsFragment.getClients());
        setResult(Activity.RESULT_OK, intent);
        finish();
    }
}

