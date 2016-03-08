package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Snapcast implements JsonSerialisable {
    String name = "";
    String version = "";
    int protocolVersion = 1;

    public Snapcast() {
    }

    public Snapcast(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            name = json.getString("name");
            version = json.getString("version");
            protocolVersion = json.getInt("protocolVersion");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("name", name);
            json.put("version", version);
            json.put("protocolVersion", protocolVersion);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public String getName() {
        return name;
    }

    public String getVersion() {
        return version;
    }

    public int getProtocolVersion() {
        return protocolVersion;
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Snapcast that = (Snapcast) o;

        if (name != null ? !name.equals(that.name) : that.name != null) return false;
        if (version != null ? !version.equals(that.version) : that.version != null) return false;
        return (protocolVersion == that.protocolVersion);
    }

    @Override
    public int hashCode() {
        int result = name != null ? name.hashCode() : 0;
        result = 31 * result + (version != null ? version.hashCode() : 0);
        result = 31 * result + protocolVersion;
        return result;
    }
}

