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
public class Stream implements JsonSerialisable {
    private StreamUri uri;
    private String id;
    private Status status;

    public Stream(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            if (json.has("uri") && (json.get("uri") instanceof JSONObject)) {
                uri = new StreamUri(json.getJSONObject("uri"));
                id = json.getString("id");
                status = Status.fromString(json.getString("status"));
            } else {
                uri = new StreamUri(json);
                id = json.getString("id");
                status = Status.unknown;
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("uri", uri.toJson());
            json.put("id", id);
            json.put("status", status);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Stream stream = (Stream) o;

        if (uri != null ? !uri.equals(stream.uri) : stream.uri != null) return false;
        if (id != null ? !id.equals(stream.id) : stream.id != null) return false;
        return !(status != null ? !status.equals(stream.status) : stream.status != null);
    }

    @Override
    public int hashCode() {
        int result = uri != null ? uri.hashCode() : 0;
        result = 31 * result + (id != null ? id.hashCode() : 0);
        result = 31 * result + (status != null ? status.hashCode() : 0);
        return result;
    }

    public StreamUri getUri() {
        return uri;
    }

    public void setUri(StreamUri uri) {
        this.uri = uri;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public Status getStatus() {
        return status;
    }

    public void setStatus(Status status) {
        this.status = status;
    }

    public String getName() {
        return uri.getName();
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    public enum Status {
        unknown("unknown"),
        idle("idle"),
        playing("playing"),
        disabled("disabled");

        private String status;

        Status(String status) {
            this.status = status;
        }

        public static Status fromString(String status) {
            if (status != null) {
                for (Status s : Status.values()) {
                    if (status.equalsIgnoreCase(s.status)) {
                        return s;
                    }
                }
            }
            return null;
        }
    }
}
