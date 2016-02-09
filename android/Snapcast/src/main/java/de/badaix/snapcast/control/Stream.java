package de.badaix.snapcast.control;

import android.os.Bundle;
import android.os.Parcel;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by johannes on 06.01.16.
 */
public class Stream implements JsonSerialisable {
    public static final Creator<Stream> CREATOR = new Creator<Stream>() {
        @Override
        public Stream createFromParcel(Parcel in) {
            return new Stream(in);
        }

        @Override
        public Stream[] newArray(int size) {
            return new Stream[size];
        }
    };

    private String uri;
    private String scheme;
    private String host;
    private String path;
    private String fragment;
    private String id;
    private HashMap<String, String> query;

    public Stream(JSONObject json) {
        fromJson(json);
    }

    protected Stream(Parcel in) {
        uri = in.readString();
        scheme = in.readString();
        host = in.readString();
        path = in.readString();
        fragment = in.readString();
        id = in.readString();
        Bundle bundle = in.readBundle();
        query = (HashMap<String, String>) bundle.getSerializable("query");
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            uri = json.getString("uri");
            scheme = json.getString("scheme");
            host = json.getString("host");
            path = json.getString("path");
            fragment = json.getString("fragment");
            id = json.getString("id");
            query = new HashMap<>();
            JSONObject jQuery = json.getJSONObject("query");
            for (int i = 0; i < jQuery.names().length(); i++)
                query.put(jQuery.names().getString(i), jQuery.getString(jQuery.names().getString(i)));
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("uri", uri);
            json.put("scheme", scheme);
            json.put("host", host);
            json.put("path", path);
            json.put("fragment", fragment);
            json.put("id", id);
            JSONObject jQuery = new JSONObject();
            for (Map.Entry<String, String> entry : query.entrySet())
                jQuery.put(entry.getKey(), entry.getValue());
            json.put("query", query);
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
        if (scheme != null ? !scheme.equals(stream.scheme) : stream.scheme != null) return false;
        if (host != null ? !host.equals(stream.host) : stream.host != null) return false;
        if (path != null ? !path.equals(stream.path) : stream.path != null) return false;
        if (fragment != null ? !fragment.equals(stream.fragment) : stream.fragment != null)
            return false;
        if (id != null ? !id.equals(stream.id) : stream.id != null) return false;
        return !(query != null ? !query.equals(stream.query) : stream.query != null);
    }

    @Override
    public int hashCode() {
        int result = uri != null ? uri.hashCode() : 0;
        result = 31 * result + (scheme != null ? scheme.hashCode() : 0);
        result = 31 * result + (host != null ? host.hashCode() : 0);
        result = 31 * result + (path != null ? path.hashCode() : 0);
        result = 31 * result + (fragment != null ? fragment.hashCode() : 0);
        result = 31 * result + (id != null ? id.hashCode() : 0);
        result = 31 * result + (query != null ? query.hashCode() : 0);
        return result;
    }

    public String getUri() {
        return uri;
    }

    public void setUri(String uri) {
        this.uri = uri;
    }

    public String getScheme() {
        return scheme;
    }

    public void setScheme(String scheme) {
        this.scheme = scheme;
    }

    public String getHost() {
        return host;
    }

    public void setHost(String host) {
        this.host = host;
    }

    public String getPath() {
        return path;
    }

    public void setPath(String path) {
        this.path = path;
    }

    public String getFragment() {
        return fragment;
    }

    public void setFragment(String fragment) {
        this.fragment = fragment;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public HashMap<String, String> getQuery() {
        return query;
    }

    public void setQuery(HashMap<String, String> query) {
        this.query = query;
    }

    public String getName() {
        if (query.containsKey("name"))
            return query.get("name");
        else
            return "";
    }

    @Override
    public String toString() {
        return "Stream{" +
                "uri='" + uri + '\'' +
                ", scheme='" + scheme + '\'' +
                ", host='" + host + '\'' +
                ", path='" + path + '\'' +
                ", fragment='" + fragment + '\'' +
                ", id='" + id + '\'' +
                ", query=" + query +
                '}';
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(uri);
        dest.writeString(scheme);
        dest.writeString(host);
        dest.writeString(path);
        dest.writeString(fragment);
        dest.writeString(id);
        Bundle bundle = new Bundle();
        bundle.putSerializable("query", query);
        dest.writeBundle(bundle);
    }
}
