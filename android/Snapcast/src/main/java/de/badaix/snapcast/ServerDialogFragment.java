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

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;

import de.badaix.snapcast.utils.NsdHelper;

/**
 * Created by johannes on 21.02.16.
 */
public class ServerDialogFragment extends DialogFragment implements View.OnClickListener {

    private Button btnScan;
    private EditText editHost;
    private EditText editStreamPort;
    private EditText editControlPort;
    private CheckBox checkBoxAutoStart;
    private String host = "";
    private int streamPort = 1704;
    private int controlPort = 1705;
    private boolean autoStart = false;
    private ServerDialogListener listener = null;

    public void setListener(ServerDialogListener listener) {
        this.listener = listener;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        // Get the layout inflater
        LayoutInflater inflater = getActivity().getLayoutInflater();
        // Inflate and set the layout for the dialog
        // Pass null as the parent view because its going in the dialog layout
        View view = inflater.inflate(R.layout.dialog_server, null);
        btnScan = (Button) view.findViewById(R.id.btn_scan);
        btnScan.setOnClickListener(this);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN)
            btnScan.setVisibility(View.GONE);

        editHost = (EditText) view.findViewById(R.id.host);
        editStreamPort = (EditText) view.findViewById(R.id.stream_port);
        editControlPort = (EditText) view.findViewById(R.id.control_port);
        checkBoxAutoStart = (CheckBox) view.findViewById(R.id.checkBoxAutoStart);
        update();

        builder.setView(view)
                // Add action buttons
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        // sign in the user ...
                        host = editHost.getText().toString();
                        try {
                            streamPort = Integer.parseInt(editStreamPort.getText().toString());
                            controlPort = Integer.parseInt(editControlPort.getText().toString());
                        } catch (NumberFormatException e) {
                            e.printStackTrace();
                        }
                        if (listener != null) {
                            listener.onHostChanged(host, streamPort, controlPort);
                            listener.onAutoStartChanged(checkBoxAutoStart.isChecked());
                        }
                    }
                })
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        ServerDialogFragment.this.getDialog().cancel();
                    }
                })
                .setTitle(R.string.server_host)
                .setCancelable(false);
        return builder.create();
    }

    @Override
    public void onClick(View v) {
        NsdHelper.getInstance(getContext()).startListening("_snapcast._tcp.", "Snapcast", new NsdHelper.NsdHelperListener() {
            @Override
            public void onResolved(NsdHelper nsdHelper, NsdServiceInfo serviceInfo) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
                    setHost(serviceInfo.getHost().getCanonicalHostName(), serviceInfo.getPort(), serviceInfo.getPort() + 1);
                }
            }
        });
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        if (context instanceof Activity) {
            update();
        }

    }

    private void update() {
        if (this.getActivity() == null)
            return;

        try {
            this.getActivity().runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        editHost.setText(host);
                        editStreamPort.setText(Integer.toString(streamPort));
                        editControlPort.setText(Integer.toString(controlPort));
                        checkBoxAutoStart.setChecked(autoStart);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            });
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void setHost(String host, int streamPort, int controlPort) {
        this.host = host;
        this.streamPort = streamPort;
        this.controlPort = controlPort;
        update();
    }

    public void setAutoStart(boolean autoStart) {
        this.autoStart = autoStart;
        update();
    }


    public interface ServerDialogListener {
        void onHostChanged(String host, int streamPort, int controlPort);

        void onAutoStartChanged(boolean autoStart);
    }
}
