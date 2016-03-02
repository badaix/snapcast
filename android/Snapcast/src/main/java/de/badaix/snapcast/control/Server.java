package de.badaix.snapcast.control;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 02.03.16.
 */
public class Server implements JsonSerialisable {
    private String host;
    private String version;

    public Server(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            host = json.getString("host");
            version = json.getString("version");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("host", host);
            json.put("version", version);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public String getHost() {
        return host;
    }

    public String getVersion() {
        return version;
    }

    @Override
    public String toString() {
        return "Server{" +
                "host='" + host + '\'' +
                ", version='" + version + '\'' +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Server server = (Server) o;

        if (host != null ? !host.equals(server.host) : server.host != null) return false;
        return version != null ? version.equals(server.version) : server.version == null;

    }

    @Override
    public int hashCode() {
        int result = host != null ? host.hashCode() : 0;
        result = 31 * result + (version != null ? version.hashCode() : 0);
        return result;
    }
}

