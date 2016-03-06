package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

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
            uri = new StreamUri(json.getJSONObject("uri"));
            id = json.getString("id");
            status = Status.fromString(json.getString("status"));
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
        return "Stream{" +
                "uri='" + uri + '\'' +
                ", id='" + id + '\'' +
                ", status=" + status +
                '}';
    }

    public enum Status {
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
