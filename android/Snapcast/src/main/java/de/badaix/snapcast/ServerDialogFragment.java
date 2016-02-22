package de.badaix.snapcast;

import android.app.Dialog;
import android.content.DialogInterface;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
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
    private String host = "";
    private int streamPort = 1704;
    private int controlPort = 1705;
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
        update();

        builder.setView(view)
                // Add action buttons
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        // sign in the user ...
                        host = editHost.getText().toString();
                        streamPort = Integer.parseInt(editStreamPort.getText().toString());
                        controlPort = Integer.parseInt(editControlPort.getText().toString());
                        if (listener != null)
                            listener.onHostChanged(host, streamPort, controlPort);
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

    private void update() {
        try {
            this.getActivity().runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        editHost.setText(host);
                        editStreamPort.setText(Integer.toString(streamPort));
                        editControlPort.setText(Integer.toString(controlPort));
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

    public String getHostName() {
        return host;
    }

    public int getStreamPort() {
        return streamPort;
    }

    public int getControlPort() {
        return controlPort;
    }

    public interface ServerDialogListener {
        void onHostChanged(String host, int streamPort, int controlPort);
    }
}
