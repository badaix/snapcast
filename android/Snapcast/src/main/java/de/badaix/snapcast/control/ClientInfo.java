package de.badaix.snapcast.control;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class ClientInfo implements JsonSerialisable {
    private String mac;
    private String ip;
    private String host;
    private String version;
    private String name;
    private Volume volume;
    private Time_t lastSeen;
    private boolean connected;
    private int latency = 0;

    public ClientInfo(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            mac = json.getString("MAC");
            ip = json.getString("IP");
            host = json.getString("host");
            version = json.getString("version");
            name = json.getString("name");
            volume = new Volume(json.getJSONObject("volume"));
            lastSeen = new Time_t(json.getJSONObject("lastSeen"));
            connected = json.getBoolean("connected");
            latency = json.getInt("latency");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("MAC", mac);
            json.put("IP", ip);
            json.put("host", host);
            json.put("version", version);
            json.put("name", name);
            json.put("volume", volume);
            json.put("lastSeen", lastSeen);
            json.put("connected", connected);
            json.put("latency", latency);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public Volume getVolume() {
        return volume;
    }

    public Time_t getLastSeen() {
        return lastSeen;
    }

    public String getMac() {
        return mac;
    }

    public String getIp() {
        return ip;
    }

    public String getHost() {
        return host;
    }

    public String getVersion() {
        return version;
    }

    public String getName() {
        return name;
    }

    public int getLatency() {
        return latency;
    }

    public boolean isConnected() {
        return connected;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void setVolume(Volume volume) {
        this.volume = volume;
    }

    public void setConnected(boolean connected) {
        this.connected = connected;
    }

    public void setLatency(int latency) {
        this.latency = latency;
    }

    @Override
    public String toString() {
        return "ClientInfo{" +
                "mac='" + mac + '\'' +
                ", ip='" + ip + '\'' +
                ", host='" + host + '\'' +
                ", version='" + version + '\'' +
                ", name='" + name + '\'' +
                ", volume=" + volume +
                ", lastSeen=" + lastSeen +
                ", connected=" + connected +
                ", latency=" + latency +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        ClientInfo that = (ClientInfo) o;

        if (connected != that.connected) return false;
        if (latency != that.latency) return false;
        if (mac != null ? !mac.equals(that.mac) : that.mac != null) return false;
        if (ip != null ? !ip.equals(that.ip) : that.ip != null) return false;
        if (host != null ? !host.equals(that.host) : that.host != null) return false;
        if (version != null ? !version.equals(that.version) : that.version != null) return false;
        if (name != null ? !name.equals(that.name) : that.name != null) return false;
        return !(volume != null ? !volume.equals(that.volume) : that.volume != null);

    }

    @Override
    public int hashCode() {
        int result = mac != null ? mac.hashCode() : 0;
        result = 31 * result + (ip != null ? ip.hashCode() : 0);
        result = 31 * result + (host != null ? host.hashCode() : 0);
        result = 31 * result + (version != null ? version.hashCode() : 0);
        result = 31 * result + (name != null ? name.hashCode() : 0);
        result = 31 * result + (volume != null ? volume.hashCode() : 0);
        result = 31 * result + (connected ? 1 : 0);
        result = 31 * result + latency;
        return result;
    }
}

