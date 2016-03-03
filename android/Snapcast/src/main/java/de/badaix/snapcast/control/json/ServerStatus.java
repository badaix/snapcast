package de.badaix.snapcast.control.json;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

/**
 * Created by johannes on 06.01.16.
 */
public class ServerStatus implements JsonSerialisable {
    private ArrayList<Client> clients = new ArrayList<Client>();
    private ArrayList<Stream> streams = new ArrayList<Stream>();
    private Server server = null;

    public ServerStatus(JSONObject json) {
        fromJson(json);
    }

    public ServerStatus() {
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            clear();
            server = new Server(json.getJSONObject("server"));
            JSONArray jStreams = json.getJSONArray("streams");
            for (int i = 0; i < jStreams.length(); i++)
                streams.add(new Stream(jStreams.getJSONObject(i)));
            JSONArray jClients = json.getJSONArray("clients");
            for (int i = 0; i < jClients.length(); i++)
                clients.add(new Client(jClients.getJSONObject(i)));
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("server", server.toJson());
            json.put("streams", getJsonStreams());
            json.put("clients", getJsonClients());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public void clear() {
        clients.clear();
        streams.clear();
    }

    public boolean removeClient(Client client) {
        for (int i = 0; i < clients.size(); ++i) {
            if (clients.get(i).getMac().equals(client.getMac())) {
                clients.remove(i);
                return true;
            }
        }
        return false;
    }

    public boolean updateClient(Client client) {
        if (client == null)
            return false;

        for (int i = 0; i < clients.size(); ++i) {
            Client clientInfo = clients.get(i);
            if (clientInfo == null)
                continue;

            if (client.getMac().equals(clientInfo.getMac())) {
                if (clientInfo.equals(client))
                    return false;
                clients.set(i, client);
                return true;
            }
        }
        clients.add(client);
        return true;
    }

    public boolean updateStream(Stream stream) {
        if (stream == null)
            return false;

        for (int i = 0; i < streams.size(); ++i) {
            Stream s = streams.get(i);
            if (s == null)
                continue;


            if (stream.getUri().equals(s.getUri())) {
                if (stream.equals(stream))
                    return false;
                streams.set(i, stream);
                return true;
            }
        }
        streams.add(stream);
        return true;
    }

    public ArrayList<Client> getClientInfos() {
        return clients;
    }

    public ArrayList<Stream> getStreams() {
        return streams;
    }

    public JSONArray getJsonStreams() {
        JSONArray jsonArray = new JSONArray();
        for (Stream stream : streams)
            jsonArray.put(stream.toJson());
        return jsonArray;
    }

    public JSONArray getJsonClients() {
        JSONArray jsonArray = new JSONArray();
        for (Client client : clients)
            jsonArray.put(client.toJson());
        return jsonArray;
    }

}
