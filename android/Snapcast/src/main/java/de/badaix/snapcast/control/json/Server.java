package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 02.03.16.
 */
public class Server implements JsonSerialisable {
    private Host host;
    private Snapcast snapserver;

    public Server(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            if (json.has("host") && !(json.get("host") instanceof String))
                host = new Host(json.getJSONObject("host"));
            else {
                host = new Host();
                host.name = json.getString("host");
            }

            if (json.has("snapserver"))
                snapserver = new Snapcast(json.getJSONObject("snapserver"));
            else {
                snapserver = new Snapcast();
                snapserver.version = json.getString("version");
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("host", host.toJson());
            json.put("snapserver", snapserver.toJson());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public Host getHost() {
        return host;
    }

    public Snapcast getSnapserver() {
        return snapserver;
    }

    @Override
    public String toString() {
        return "Server{" +
                "host='" + host + '\'' +
                ", snapserver='" + snapserver + '\'' +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Server server = (Server) o;

        if (host != null ? !host.equals(server.host) : server.host != null) return false;
        return snapserver != null ? snapserver.equals(server.snapserver) : server.snapserver == null;

    }

    @Override
    public int hashCode() {
        int result = host != null ? host.hashCode() : 0;
        result = 31 * result + (snapserver != null ? snapserver.hashCode() : 0);
        return result;
    }
}

