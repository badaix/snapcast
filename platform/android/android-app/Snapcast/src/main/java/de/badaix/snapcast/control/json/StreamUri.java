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

import java.util.HashMap;
import java.util.Map;

/**
 * Created by johannes on 06.01.16.
 */
public class StreamUri implements JsonSerialisable {
    private String raw;
    private String scheme;
    private String host;
    private String path;
    private String fragment;
    private HashMap<String, String> query;

    public StreamUri(JSONObject json) {
        fromJson(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            if (json.has("raw"))
                raw = json.getString("raw");
            else if (json.has("uri"))
                raw = json.getString("uri");
            scheme = json.getString("scheme");
            host = json.getString("host");
            path = json.getString("path");
            fragment = json.getString("fragment");
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
            json.put("raw", raw);
            json.put("scheme", scheme);
            json.put("host", host);
            json.put("path", path);
            json.put("fragment", fragment);
            JSONObject jQuery = new JSONObject();
            for (Map.Entry<String, String> entry : query.entrySet())
                jQuery.put(entry.getKey(), entry.getValue());
            json.put("query", jQuery);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        StreamUri stream = (StreamUri) o;

        if (raw != null ? !raw.equals(stream.raw) : stream.raw != null) return false;
        if (scheme != null ? !scheme.equals(stream.scheme) : stream.scheme != null) return false;
        if (host != null ? !host.equals(stream.host) : stream.host != null) return false;
        if (path != null ? !path.equals(stream.path) : stream.path != null) return false;
        if (fragment != null ? !fragment.equals(stream.fragment) : stream.fragment != null)
            return false;
        return !(query != null ? !query.equals(stream.query) : stream.query != null);
    }

    @Override
    public int hashCode() {
        int result = raw != null ? raw.hashCode() : 0;
        result = 31 * result + (scheme != null ? scheme.hashCode() : 0);
        result = 31 * result + (host != null ? host.hashCode() : 0);
        result = 31 * result + (path != null ? path.hashCode() : 0);
        result = 31 * result + (fragment != null ? fragment.hashCode() : 0);
        result = 31 * result + (query != null ? query.hashCode() : 0);
        return result;
    }

    public String getRaw() {
        return raw;
    }

    public void setUri(String uri) {
        this.raw = raw;
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
        return toJson().toString();
    }
}
