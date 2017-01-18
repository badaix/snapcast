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
 * Created by johannes on 02.03.16.
 */
public class Server implements JsonSerialisable {
    private Host host;
    private Snapserver snapserver;

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
                snapserver = new Snapserver(json.getJSONObject("snapserver"));
            else {
                snapserver = new Snapserver();
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
        return toJson().toString();
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

