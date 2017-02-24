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

    public enum RPCEvent {
        response,
        notification
    }

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
                RPCResponse response = new RPCResponse(json);
//                Log.d(TAG, "ID: " + json.getString("id"));
                RPCRequest request = null;
                synchronized (pendingRequests) {
                    if (pendingRequests.containsKey(response.id)) {
                        request = new RPCRequest(new JSONObject(pendingRequests.get(response.id)));
                        Log.d(TAG, "Response to: " + request.method);
                        pendingRequests.remove(response.id);
                    }
                }

                if (listener == null)
                    return;

                if (json.has("error")) {
                    JSONObject error = json.getJSONObject("error");
                    Log.e(TAG, "error " + error.getInt("code") + ": " + error.getString("message") + "; data: " + error.getString("data"));
                }

                if (request == null) {
                    Log.e(TAG, "request for id " + response.id + " not found");
                    return;
                }

                RPCEvent rpcEvent = RPCEvent.response;
                /// Response to a "Object.GetStatus" message
                if (request.method.equals("Client.GetStatus")) {
                    listener.onUpdate(new Client(json.getJSONObject("result")));
                } else if (request.method.equals("Client.SetVolume")) {
                } else if (request.method.equals("Client.SetLatency")) {
                } else if (request.method.equals("Client.SetName")) {
                } else if (request.method.equals("Group.GetStatus")) {
                    listener.onUpdate(new Group(json.getJSONObject("result")));
                } else if (request.method.equals("Group.SetMute")) {
//                    listener.onMute(rpcEvent, request.params.getString("id"), response.result.getBoolean("mute"));
                } else if (request.method.equals("Group.SetStream")) {
                } else if (request.method.equals("Group.SetClients")) {
                    listener.onUpdate(new ServerStatus(json.getJSONObject("result").getJSONObject("server")));
                } else if (request.method.equals("Server.GetStatus")) {
                    listener.onUpdate(new ServerStatus(json.getJSONObject("result")));
                } else if (request.method.equals("Server.DeleteClient")) {
                    listener.onUpdate(new ServerStatus(json.getJSONObject("result").getJSONObject("server")));
                }
            } else {
                /// Notification
                if (listener == null)
                    return;
                RPCEvent rpcEvent = RPCEvent.notification;
                RPCNotification notification = new RPCNotification(json);

                if (notification.method.equals("Client.OnConnect")) {
                    listener.onConnect(new Client(notification.params.getJSONObject("client")));
                } else if (notification.method.equals("Client.OnDisconnect")) {
                    listener.onDisconnect(notification.params.getString("id"));
                } else if (notification.method.equals("Client.OnVolumeChanged")) {
                    listener.onVolumeChanged(rpcEvent, notification.params.getString("id"), new Volume(notification.params.getJSONObject("volume")));
                } else if (notification.method.equals("Client.OnLatencyChanged")) {
                    listener.onLatencyChanged(rpcEvent, notification.params.getString("id"), notification.params.getInt("latency"));
                } else if (notification.method.equals("Client.OnNameChanged")) {
                    listener.onNameChanged(rpcEvent, notification.params.getString("id"), notification.params.getString("name"));
                } else if (notification.method.equals("Group.OnMute")) {
                    listener.onMute(rpcEvent, notification.params.getString("id"), notification.params.getBoolean("mute"));
                } else if (notification.method.equals("Group.OnStreamChanged")) {
                    listener.onStreamChanged(rpcEvent, notification.params.getString("id"), notification.params.getString("stream_id"));
                } else if (notification.method.equals("Stream.OnUpdate")) {
                    listener.onUpdate(notification.params.getString("id"), new Stream(notification.params.getJSONObject("stream")));
                } else if (notification.method.equals("Group.OnUpdate")) {
                    listener.onUpdate(new Group(notification.params.getJSONObject("group")));
                } else if (notification.method.equals("Server.OnUpdate")) {
                    listener.onUpdate(new ServerStatus(notification.params.getJSONObject("server")));
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

    private RPCRequest jsonRequest(String method, JSONObject params) {
        RPCRequest request = new RPCRequest(method, msgId, params);
        synchronized (pendingRequests) {
            pendingRequests.put(msgId, request.toString());
        }
        msgId++;
        return request;
    }

    public void getServerStatus() {
        RPCRequest request = jsonRequest("Server.GetStatus", null);
        tcpClient.sendMessage(request.toString());
    }

    public void setName(Client client, String name) {
        try {
            JSONObject params = new JSONObject();
            params.put("id", client.getId());
            params.put("name", name);
            RPCRequest request = jsonRequest("Client.SetName", params);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setLatency(Client client, int latency) {
        try {
            JSONObject params = new JSONObject();
            params.put("id", client.getId());
            params.put("latency", latency);
            RPCRequest request = jsonRequest("Client.SetLatency", params);
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
            JSONObject params = new JSONObject();
            params.put("id", groupId);
            params.put("clients", clients);
            RPCRequest request = jsonRequest("Group.SetClients", params);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setStream(String groupId, String streamId) {
        try {
            JSONObject params = new JSONObject();
            params.put("id", groupId);
            params.put("stream_id", streamId);
            RPCRequest request = jsonRequest("Group.SetStream", params);
            tcpClient.sendMessage(request.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void setGroupMuted(Group group, boolean muted) {
        try {
            JSONObject params = new JSONObject();
            params.put("id", group.getId());
            params.put("mute", muted);
            RPCRequest request = jsonRequest("Group.SetMute", params);
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
                RPCRequest volumeRequest = getVolumeRequest(client, volume.getPercent(), volume.isMuted());
                batch.put(volumeRequest.toJson());
            }
            tcpClient.sendMessage(batch.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    private RPCRequest getVolumeRequest(Client client, int percent, boolean mute) throws JSONException {
        Volume volume = new Volume(percent, mute);
        JSONObject params = new JSONObject();
        params.put("id", client.getId());
        params.put("volume", volume.toJson());
        return jsonRequest("Client.SetVolume", params);
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
            JSONObject params = new JSONObject();
            params.put("id", client.getId());
            RPCRequest request = jsonRequest("Server.DeleteClient", params);
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


    public interface ClientListener {
        void onConnect(Client client);
        void onDisconnect(String clientId);
        void onUpdate(Client client);
        void onVolumeChanged(RPCEvent event, String clientId, Volume volume);
        void onLatencyChanged(RPCEvent event, String clientId, long latency);
        void onNameChanged(RPCEvent event, String clientId, String name);
    }

    public interface GroupListener {
        void onUpdate(Group group);
        void onMute(RPCEvent event, String groupId, boolean mute);
        void onStreamChanged(RPCEvent event, String groupId, String streamId);
    }

    public interface StreamListener {
        void onUpdate(String streamId, Stream stream);
    }

    public interface ServerListener {
        void onUpdate(ServerStatus server);
    }



    public interface RemoteControlListener extends ServerListener, StreamListener, GroupListener, ClientListener {
        void onConnected(RemoteControl remoteControl);

        void onConnecting(RemoteControl remoteControl);

        void onDisconnected(RemoteControl remoteControl, Exception e);

        void onBatchStart();
        void onBatchEnd();
/*
        void onClientEvent(RemoteControl remoteControl, RPCEvent rpcEvent, Client client, ClientEvent event);

        void onServerUpdate(RemoteControl remoteControl, RPCEvent rpcEvent, ServerStatus serverStatus);

        void onStreamUpdate(RemoteControl remoteControl, RPCEvent rpcEvent, Stream stream);

        void onGroupUpdate(RemoteControl remoteControl, RPCEvent rpcEvent, Group group);
*/
    }
}
