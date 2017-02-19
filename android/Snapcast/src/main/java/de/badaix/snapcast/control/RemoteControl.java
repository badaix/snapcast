/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2017  Johannes Pohl
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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.Group;
import de.badaix.snapcast.control.json.ServerStatus;
import de.badaix.snapcast.control.json.Stream;
import de.badaix.snapcast.control.json.Volume;

/**
 * Created by johannes on 13.01.16.
 */
public class RemoteControl implements TcpClient.TcpClientListener {

    private static final String TAG = "RC";

    private TcpClient tcpClient;
    private long msgId;
    private RemoteControlListener listener;
    private String host;
    private int port;
    private final HashMap<Long, String> pendingRequests;

    public RemoteControl(RemoteControlListener listener) {
        this.listener = listener;
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
        try {
            if (message.trim().startsWith("[")) {
                JSONArray jsonArray = new JSONArray(message);
                for (int i = 0; i < jsonArray.length(); ++i) {
                    JSONObject json = jsonArray.getJSONObject(i);
                    processJson(json);
                }
            } else {
                JSONObject json = new JSONObject(message);
                processJson(json);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    private void processJson(JSONObject json) {
//        Log.d(TAG, "Msg received: " + message);
        try {

            if (json.has("id")) {
                /// Response
//                Log.d(TAG, "ID: " + json.getString("id"));
                long id = json.getLong("id");
                String requestMethod = "";
                synchronized (pendingRequests) {
                    if (pendingRequests.containsKey(id)) {
                        requestMethod = pendingRequests.get(id);
//                        Log.d(TAG, "Response to: " + request);
                        pendingRequests.remove(id);
                    }
                }

                if (listener == null)
                    return;

                if (json.has("error")) {
                    JSONObject error = json.getJSONObject("error");
                    Log.e(TAG, "error " + error.getInt("code") + ": " + error.getString("message") + "; data: " + error.getString("data"));
                }

                if (TextUtils.isEmpty(requestMethod)) {
                    Log.e(TAG, "request for id " + id + " not found");
                    return;
                }

                RpcEvent rpcEvent = RpcEvent.response;
                /// Response to a "Object.GetStatus" message
                if (requestMethod.equals("Client.GetStatus")) {
                    listener.onClientEvent(this, rpcEvent, new Client(json.getJSONObject("result")), ClientEvent.updated);
                } else if (requestMethod.equals("Client.SetVolume")) {
                } else if (requestMethod.equals("Client.SetLatency")) {
                } else if (requestMethod.equals("Client.SetName")) {

                } else if (requestMethod.equals("Group.GetStatus")) {
                    listener.onGroupUpdate(this, rpcEvent, new Group(json.getJSONObject("result")));
                } else if (requestMethod.equals("Group.SetMute")) {
                } else if (requestMethod.equals("Group.SetStream")) {
                } else if (requestMethod.equals("Group.SetClients")) {
                    listener.onServerUpdate(this, rpcEvent, new ServerStatus(json.getJSONObject("result").getJSONObject("server")));
                } else if (requestMethod.equals("Server.GetStatus")) {
                    listener.onServerUpdate(this, rpcEvent, new ServerStatus(json.getJSONObject("result")));
                } else if (requestMethod.equals("Server.DeleteClient")) {
//                } else if (json.getJSONObject("result").has("method") && json.getJSONObject("result").has("params")) {
                    /// Response to a "Object.Set" message
                    JSONObject result = json.getJSONObject("result");
                    String method = result.getString("method");
                    if ("Client.OnUpdate".equals(method)) {
                        listener.onClientEvent(this, rpcEvent, new Client(result.getJSONObject("params")), ClientEvent.updated);
                    } else if ("Group.OnUpdate".equals(method)) {
                        listener.onGroupUpdate(this, rpcEvent, new Group(result.getJSONObject("params")));
                    } else if ("Server.OnUpdate".equals(method)) {
                        listener.onServerUpdate(this, rpcEvent, new ServerStatus(result.getJSONObject("params")));
                    }
                }
            } else {
                /// Notification
                if (listener == null)
                    return;
                RpcEvent rpcEvent = RpcEvent.notification;
                String method = json.getString("method");

                if (method.equals("Client.OnConnect")) {
                } else if (method.equals("Client.OnDisconnect")) {
                } else if (method.equals("Client.OnVolumeChanged")) {
                } else if (method.equals("Client.OnLatencyChanged")) {
                } else if (method.equals("Client.OnNameChanged")) {

                } else if (method.equals("Group.OnMute")) {
                } else if (method.equals("Group.OnStreamChanged")) {

                } else if (method.equals("Stream.OnUpdate")) {

                } else if (method.equals("Server.OnUpdate")) {

                } else if (method.contains("Client.On")) {
                    listener.onClientEvent(this, rpcEvent, new Client(json.getJSONObject("params").getJSONObject("client")), ClientEvent.fromString(method));
                } else if (method.equals("Stream.OnUpdate")) {
                    listener.onStreamUpdate(this, rpcEvent, new Stream(json.getJSONObject("params").getJSONObject("stream")));
                } else if (method.equals("Group.OnUpdate")) {
                    listener.onGroupUpdate(this, rpcEvent, new Group(json.getJSONObject("params").getJSONObject("group")));
                } else if (method.equals("Server.OnUpdate")) {
                    listener.onServerUpdate(this, rpcEvent, new ServerStatus(json.getJSONObject("params").getJSONObject("server")));
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
        if (listener != null)
            listener.onConnected(this);
    }

    @Override
    public void onDisconnected(TcpClient tcpClient, Exception e) {
        Log.d(TAG, "onDisconnected");
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
            JSONObject body = new JSONObject();
            body.put("id", client.getId());
            body.put("name", name);
            JSONObject request = jsonRequest("Client.SetName", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setLatency(Client client, int latency) {
        try {
            JSONObject body = new JSONObject();
            body.put("id", client.getId());
            body.put("latency", latency);
            JSONObject request = jsonRequest("Client.SetLatency", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setStream(Group group, String id) {
        setStream(group.getId(), id);
    }

    public void setClients(String groupId, ArrayList<String> clientIds) {
        try {
            JSONArray clients = new JSONArray();
            for (String clientId : clientIds)
                clients.put(clientId);
            JSONObject body = new JSONObject();
            body.put("id", groupId);
            body.put("clients", clients);
            JSONObject request = jsonRequest("Group.SetClients", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setStream(String groupId, String streamId) {
        try {
            JSONObject body = new JSONObject();
            body.put("id", groupId);
            body.put("stream_id", streamId);
            JSONObject request = jsonRequest("Group.SetStream", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setGroupMuted(Group group, boolean muted) {
        try {
            JSONObject body = new JSONObject();
            body.put("id", group.getId());
            body.put("mute", muted);
            JSONObject request = jsonRequest("Group.SetMute", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setGroupVolume(Group group) {
        try {
            JSONArray batch = new JSONArray();
            for (Client client : group.getClients()) {
                Volume volume = client.getConfig().getVolume();
                JSONObject volumeRequest = getVolumeRequest(client, volume.getPercent(), volume.isMuted());
                batch.put(volumeRequest);
            }
            tcpClient.sendMessage(batch.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    private JSONObject getVolumeRequest(Client client, int percent, boolean mute) throws JSONException {
        Volume volume = new Volume(percent, mute);
        JSONObject body = new JSONObject();
        body.put("id", client.getId());
        body.put("volume", volume.toJson());
        return jsonRequest("Client.SetVolume", body);
    }

    public void setVolume(Client client, int percent, boolean mute) {
        try {
            tcpClient.sendMessage(getVolumeRequest(client, percent, mute).toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void delete(Client client) {
        try {
            JSONObject body = new JSONObject();
            body.put("id", client.getId());
            JSONObject request = jsonRequest("Server.DeleteClient", body);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public enum ClientEvent {
        connected("Client.OnConnect"),
        disconnected("Client.OnDisconnect"),
        updated("Client.OnUpdate");
        //deleted("Client.OnDelete");
        private String text;

        ClientEvent(String text) {
            this.text = text;
        }

        public static ClientEvent fromString(String text) {
            if (text != null) {
                for (ClientEvent b : ClientEvent.values()) {
                    if (text.equalsIgnoreCase(b.text)) {
                        return b;
                    }
                }
            }
            throw new IllegalArgumentException("No ClientEvent with text " + text + " found");
        }

        public String getText() {
            return this.text;
        }
    }

    public enum RpcEvent {
        response,
        notification
    }

    public interface RemoteControlListener {
        void onConnected(RemoteControl remoteControl);

        void onConnecting(RemoteControl remoteControl);

        void onDisconnected(RemoteControl remoteControl, Exception e);

        void onClientEvent(RemoteControl remoteControl, RpcEvent rpcEvent, Client client, ClientEvent event);

        void onServerUpdate(RemoteControl remoteControl, RpcEvent rpcEvent, ServerStatus serverStatus);

        void onStreamUpdate(RemoteControl remoteControl, RpcEvent rpcEvent, Stream stream);

        void onGroupUpdate(RemoteControl remoteControl, RpcEvent rpcEvent, Group group);
    }
}
