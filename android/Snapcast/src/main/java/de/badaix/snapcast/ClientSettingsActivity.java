package de.badaix.snapcast;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import de.badaix.snapcast.ClientSettingsFragment;
import de.badaix.snapcast.control.ClientInfo;

/**
 * Created by johannes on 11.01.16.
 */
public class ClientSettingsActivity extends AppCompatActivity {
    private ClientSettingsFragment clientSettingsFragment = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ClientInfo clientInfo = (ClientInfo)getIntent().getParcelableExtra("clientInfo");
        clientSettingsFragment = new ClientSettingsFragment();
        Bundle bundle = new Bundle();
        bundle.putParcelable("clientInfo", clientInfo);
        clientSettingsFragment.setArguments(bundle);
//        clientSettingsFragment.setClientInfo((ClientInfo)(getIntent().getParcelableExtra("clientInfo")));
        // Display the fragment as the main content.
        getFragmentManager().beginTransaction()
                .replace(android.R.id.content, clientSettingsFragment)
                .commit();
    }

    @Override
    public void onBackPressed() {
        Intent intent = new Intent();
        intent.putExtra("clientInfo", clientSettingsFragment.getClientInfo());
        setResult(Activity.RESULT_OK, intent);
        finish();
//        super.onBackPressed();
    }
}
