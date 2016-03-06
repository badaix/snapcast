package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Snapcast implements JsonSerialisable {
    String name = "";
    String version = "";
    int streamProtocolVersion = 1;
    int controlProtocolVersion = 1;

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
            streamProtocolVersion = json.getInt("streamProtocolVersion");
            controlProtocolVersion = json.getInt("controlProtocolVersion");
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
            json.put("streamProtocolVersion", streamProtocolVersion);
            json.put("controlProtocolVersion", controlProtocolVersion);
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

    public int getStreamProtocolVersion() {
        return streamProtocolVersion;
    }

    public int getControlProtocolVersion() {
        return controlProtocolVersion;
    }

    @Override
    public String toString() {
        return "Client{" +
                "name='" + name + '\'' +
                ", version='" + version + '\'' +
                ", streamProtocolVersion=" + streamProtocolVersion +
                ", controlProtocolVersion=" + controlProtocolVersion +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Snapcast that = (Snapcast) o;

        if (name != null ? !name.equals(that.name) : that.name != null) return false;
        if (version != null ? !version.equals(that.version) : that.version != null) return false;
        if (streamProtocolVersion != that.streamProtocolVersion) return false;
        return (controlProtocolVersion == that.controlProtocolVersion);
    }

    @Override
    public int hashCode() {
        int result = name != null ? name.hashCode() : 0;
        result = 31 * result + (version != null ? version.hashCode() : 0);
        result = 31 * result + streamProtocolVersion;
        result = 31 * result + controlProtocolVersion;
        return result;
    }
}

