package de.badaix.snapcast.control;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 13.01.16.
 */
public class RemoteControl implements TcpClient.TcpClientListener {

    private static final String TAG = "RC";

    private TcpClient tcpClient;
    private long msgId;
    private RemoteControlListener listener;
    private ServerInfo serverInfo;
    private String host;
    private int port;

    public RemoteControl(RemoteControlListener listener) {
        this.listener = listener;
        serverInfo = new ServerInfo();
        msgId = 0;
    }

    public void connect(final String host, final int port) {
        if ((tcpClient != null) && tcpClient.isConnected()) {
            if (this.host.equals(host) && (this.port == port))
                return;
            else
                disconnect();
        }
        this.host = host;
        this.port = port;

        tcpClient = new TcpClient(this);
        tcpClient.start(host, port);
    }

    public void disconnect() {
        if ((tcpClient != null) && (tcpClient.isConnected()))
            tcpClient.stop();
        tcpClient = null;
    }

    public boolean isConnected() {
        return ((tcpClient != null) && tcpClient.isConnected());
    }

    @Override
    public void onMessageReceived(TcpClient tcpClient, String message) {
        Log.d(TAG, "Msg received: " + message);
        try {
            JSONObject json = new JSONObject(message);
            if (json.has("id")) {
                Log.d(TAG, "ID: " + json.getString("id"));
                if (json.has("error")) {
                    JSONObject error = json.getJSONObject("error");
                    Log.e(TAG, "error " + error.getInt("code") + ": " + error.getString("message"));
                } else if (json.has("result") && (json.get("result") instanceof JSONObject) &&
                        json.getJSONObject("result").has("clients")) {
                    serverInfo.clear();
                    JSONArray clients = json.getJSONObject("result").getJSONArray("clients");
                    for (int i = 0; i < clients.length(); i++) {
                        final ClientInfo clientInfo = new ClientInfo(clients.getJSONObject(i));
                        serverInfo.updateClient(clientInfo);
                    }
                    if (json.getJSONObject("result").has("streams")) {
                        JSONArray streams = json.getJSONObject("result").getJSONArray("streams");
                        for (int i = 0; i < streams.length(); i++) {
                            final Stream stream = new Stream(streams.getJSONObject(i));
                            serverInfo.updateStream(stream);
                        }
                    }
                    if (listener != null)
                        listener.onServerInfo(this, serverInfo);
                }
            } else {
                String method = json.getString("method");
                Log.d(TAG, "Notification: " + method);
                if (method.contains("Client.On")) {
                    final ClientInfo clientInfo = new ClientInfo(json.getJSONObject("params").getJSONObject("data"));
//                    serverInfo.addClient(clientInfo);
                    if (listener != null) {
                        ClientEvent event;
                        if (method.equals("Client.OnUpdate"))
                            listener.onClientEvent(this, clientInfo, ClientEvent.updated);
                        else if (method.equals("Client.OnConnect"))
                            listener.onClientEvent(this, clientInfo, ClientEvent.connected);
                        else if (method.equals("Client.OnDisconnect"))
                            listener.onClientEvent(this, clientInfo, ClientEvent.disconnected);
                        else if (method.equals("Client.OnDelete")) {
                            listener.onClientEvent(this, clientInfo, ClientEvent.deleted);
                        }
                    }
                }
            }

        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onConnected(TcpClient tcpClient) {
        Log.d(TAG, "onConnected");
        if (listener != null)
            listener.onConnected(this);
    }

    @Override
    public void onDisconnected(TcpClient tcpClient) {
        Log.d(TAG, "onDisconnected");
        if (listener != null)
            listener.onDisconnected(this);
    }

    private JSONObject jsonRequest(String method, JSONObject params) {
        JSONObject request = new JSONObject();
        try {
            request.put("jsonrpc", "2.0");
            request.put("method", method);
            request.put("id", msgId);
            if (params != null)
                request.put("params", params);
            msgId++;
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return request;
    }

    public void getServerStatus() {
        JSONObject request = jsonRequest("Server.GetStatus", null);
        tcpClient.sendMessage(request.toString());
    }

    public void setName(ClientInfo clientInfo, String name) {
        try {
            JSONObject request = jsonRequest("Client.SetName", new JSONObject("{\"client\": \"" + clientInfo.getMac() + "\", \"name\": \"" + name + "\"}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setStream(ClientInfo clientInfo, String id) {
        try {
            JSONObject request = jsonRequest("Client.SetStream", new JSONObject("{\"client\": \"" + clientInfo.getMac() + "\", \"id\": \"" + id + "\"}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setVolume(ClientInfo clientInfo, int percent) {
        try {
            JSONObject request = jsonRequest("Client.SetVolume", new JSONObject("{\"client\": \"" + clientInfo.getMac() + "\", \"volume\": " + percent + "}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setMute(ClientInfo clientInfo, boolean mute) {
        try {
            JSONObject request = jsonRequest("Client.SetMute", new JSONObject("{\"client\": \"" + clientInfo.getMac() + "\", \"mute\": " + mute + "}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void delete(ClientInfo clientInfo) {
        try {
            JSONObject request = jsonRequest("Server.DeleteClient", new JSONObject("{\"client\": \"" + clientInfo.getMac() + "\"}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public enum ClientEvent {
        connected,
        disconnected,
        updated,
        deleted
    }

    public interface RemoteControlListener {
        void onConnected(RemoteControl remoteControl);

        void onDisconnected(RemoteControl remoteControl);

        void onClientEvent(RemoteControl remoteControl, ClientInfo clientInfo, ClientEvent event);

        void onServerInfo(RemoteControl remoteControl, ServerInfo serverInfo);
    }
}
