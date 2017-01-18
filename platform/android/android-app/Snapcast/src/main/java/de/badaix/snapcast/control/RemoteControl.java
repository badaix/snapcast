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

package de.badaix.snapcast.control;

import android.text.TextUtils;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.ServerStatus;
import de.badaix.snapcast.control.json.Stream;

/**
 * Created by johannes on 13.01.16.
 */
public class RemoteControl implements TcpClient.TcpClientListener {

    private static final String TAG = "RC";

    private TcpClient tcpClient;
    private long msgId;
    private RemoteControlListener listener;
    private ServerStatus serverStatus;
    private String host;
    private int port;
    private HashMap<Long, String> pendingRequests;

    public RemoteControl(RemoteControlListener listener) {
        this.listener = listener;
        serverStatus = new ServerStatus();
        msgId = 0;
        pendingRequests = new HashMap<>();
    }

    public String getHost() {
        return host;
    }


    public int getPort() {
        return port;
    }


    public synchronized void connect(final String host, final int port) {
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
        pendingRequests.clear();
    }

    public boolean isConnected() {
        return ((tcpClient != null) && tcpClient.isConnected());
    }

    @Override
    public void onMessageReceived(TcpClient tcpClient, String message) {
//        Log.d(TAG, "Msg received: " + message);
        try {
            JSONObject json = new JSONObject(message);
            if (json.has("id")) {
//                Log.d(TAG, "ID: " + json.getString("id"));
                long id = json.getLong("id");
                String request = "";
                synchronized (pendingRequests) {
                    if (pendingRequests.containsKey(id)) {
                        request = pendingRequests.get(id);
//                        Log.d(TAG, "Response to: " + request);
                        pendingRequests.remove(id);
                    }
                }
                if (json.has("error")) {
                    JSONObject error = json.getJSONObject("error");
                    Log.e(TAG, "error " + error.getInt("code") + ": " + error.getString("message"));
                } else if (!TextUtils.isEmpty(request)) {
                    if (request.equals("Server.GetStatus")) {
                        serverStatus.fromJson(json.getJSONObject("result"));
                        if (listener != null)
                            listener.onServerStatus(this, serverStatus);
                    }
                }
            } else {
                String method = json.getString("method");
//                Log.d(TAG, "Notification: " + method);
                if (method.contains("Client.On")) {
                    final Client client = new Client(json.getJSONObject("params").getJSONObject("data"));
//                    serverStatus.addClient(client);
                    if (listener != null) {
                        ClientEvent event;
                        if (method.equals("Client.OnUpdate"))
                            listener.onClientEvent(this, client, ClientEvent.updated);
                        else if (method.equals("Client.OnConnect"))
                            listener.onClientEvent(this, client, ClientEvent.connected);
                        else if (method.equals("Client.OnDisconnect"))
                            listener.onClientEvent(this, client, ClientEvent.disconnected);
                        else if (method.equals("Client.OnDelete")) {
                            listener.onClientEvent(this, client, ClientEvent.deleted);
                        }
                    }
                } else if (method.equals("Stream.OnUpdate")) {
                    Stream stream = new Stream(json.getJSONObject("params").getJSONObject("data"));
                    listener.onStreamUpdate(this, stream);
                    Log.d(TAG, stream.toString());
                }
            }

        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onConnecting(TcpClient tcpClient) {
        Log.d(TAG, "onConnecting");
        if (listener != null)
            listener.onConnecting(this);
    }

    @Override
    public void onConnected(TcpClient tcpClient) {
        Log.d(TAG, "onConnected");
        serverStatus = new ServerStatus();
        if (listener != null)
            listener.onConnected(this);
    }

    @Override
    public void onDisconnected(TcpClient tcpClient, Exception e) {
        Log.d(TAG, "onDisconnected");
        serverStatus = null;
        if (listener != null)
            listener.onDisconnected(this, e);
    }

    private JSONObject jsonRequest(String method, JSONObject params) {
        JSONObject request = new JSONObject();
        try {
            request.put("jsonrpc", "2.0");
            request.put("method", method);
            request.put("id", msgId);
            if (params != null)
                request.put("params", params);
            synchronized (pendingRequests) {
                pendingRequests.put(msgId, method);
            }
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

    public void setName(Client client, String name) {
        try {
            JSONObject request = jsonRequest("Client.SetName", new JSONObject("{\"client\": \"" + client.getMac() + "\", \"name\": \"" + name + "\"}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setLatency(Client client, int latency) {
        try {
            JSONObject request = jsonRequest("Client.SetLatency", new JSONObject("{\"client\": \"" + client.getMac() + "\", \"latency\": " + latency + "}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setStream(Client client, String id) {
        try {
            JSONObject request = jsonRequest("Client.SetStream", new JSONObject("{\"client\": \"" + client.getMac() + "\", \"id\": \"" + id + "\"}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setVolume(Client client, int percent) {
        try {
            JSONObject request = jsonRequest("Client.SetVolume", new JSONObject("{\"client\": \"" + client.getMac() + "\", \"volume\": " + percent + "}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setMute(Client client, boolean mute) {
        try {
            JSONObject request = jsonRequest("Client.SetMute", new JSONObject("{\"client\": \"" + client.getMac() + "\", \"mute\": " + mute + "}"));
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void delete(Client client) {
        try {
            JSONObject request = jsonRequest("Server.DeleteClient", new JSONObject("{\"client\": \"" + client.getMac() + "\"}"));
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

        void onConnecting(RemoteControl remoteControl);

        void onDisconnected(RemoteControl remoteControl, Exception e);

        void onClientEvent(RemoteControl remoteControl, Client client, ClientEvent event);

        void onServerStatus(RemoteControl remoteControl, ServerStatus serverStatus);

        void onStreamUpdate(RemoteControl remoteControl, Stream stream);
    }
}
