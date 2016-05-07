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

package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class ClientConfig implements JsonSerialisable {
    String name = "";
    Volume volume;
    int latency = 0;
    String stream = "";

    public ClientConfig() {
        volume = new Volume();
    }

    public ClientConfig(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            name = json.getString("name");
            volume = new Volume(json.getJSONObject("volume"));
            latency = json.getInt("latency");
            stream = json.getString("stream");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("name", name);
            json.put("volume", volume.toJson());
            json.put("latency", latency);
            json.put("stream", stream);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public Volume getVolume() {
        return volume;
    }

    public void setVolume(Volume volume) {
        this.volume = volume;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public int getLatency() {
        return latency;
    }

    public void setLatency(int latency) {
        this.latency = latency;
    }

    public String getStream() {
        return stream;
    }

    public void setStream(String stream) {
        this.stream = stream;
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        ClientConfig that = (ClientConfig) o;

        if (latency != that.latency) return false;
        if (name != null ? !name.equals(that.name) : that.name != null) return false;
        if (stream != null ? !stream.equals(that.stream) : that.stream != null) return false;
        return !(volume != null ? !volume.equals(that.volume) : that.volume != null);

    }

    @Override
    public int hashCode() {
        int result = name != null ? name.hashCode() : 0;
        result = 31 * result + (volume != null ? volume.hashCode() : 0);
        result = 31 * result + latency;
        result = 31 * result + (stream != null ? stream.hashCode() : 0);
        return result;
    }
}

