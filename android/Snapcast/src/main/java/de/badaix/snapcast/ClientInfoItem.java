package de.badaix.snapcast;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;


import de.badaix.snapcast.control.ClientInfo;

public class ClientInfoItem extends LinearLayout {

    public static interface OnAppItemChangedListener {
        abstract void onChanged(ClientInfo clientInfo, boolean enabled);
    }

    private TextView title;
    private TextView summary;
    private ClientInfo clientInfo;

    public ClientInfoItem(Context context, ClientInfo clientInfo) {
        super(context);
        LayoutInflater vi = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        vi.inflate(R.layout.client_info, this);
        title = (TextView) findViewById(R.id.title);
        summary = (TextView) findViewById(R.id.summary);
        setClientInfo(clientInfo);
    }

    public void setClientInfo(final ClientInfo clientInfo) {
        this.clientInfo = clientInfo;
        title.setText(clientInfo.getName());
        summary.setText(clientInfo.getMac());
    }

    public ClientInfo getClientInfo() {
        return clientInfo;
    }

}
