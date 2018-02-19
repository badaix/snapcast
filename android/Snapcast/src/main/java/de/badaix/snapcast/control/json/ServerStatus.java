/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2018  Johannes Pohl
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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Created by johannes on 06.01.16.
 */
public class ServerStatus implements JsonSerialisable {
    private ArrayList<Group> groups = new ArrayList<Group>();
    private ArrayList<Stream> streams = new ArrayList<Stream>();
    private Server server = null;

    public ServerStatus(JSONObject json) {
        fromJson(json);
    }

    public ServerStatus() {
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            clear();
            server = new Server(json.getJSONObject("server"));
            JSONArray jStreams = json.getJSONArray("streams");
            for (int i = 0; i < jStreams.length(); i++)
                streams.add(new Stream(jStreams.getJSONObject(i)));
            JSONArray jGroups = json.getJSONArray("groups");
            for (int i = 0; i < jGroups.length(); i++)
                groups.add(new Group(jGroups.getJSONObject(i)));
        } catch (JSONException e) {
            e.printStackTrace();
        }
        sort();
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("server", server.toJson());
            json.put("groups", getJsonGroups());
            json.put("streams", getJsonStreams());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public void sort() {
        for (Group group : groups)
            group.sort();

        Collections.sort(groups);
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    public void clear() {
        groups.clear();
        streams.clear();
    }

    public boolean removeClient(Client client) {
        if (client == null)
            return false;

        for (Group group : groups) {
            if (group.getClient(client.getId()) != null) {
                group.removeClient(client.getId());
                return true;
            }
        }
        return false;
    }

    public boolean updateClient(Client client) {
        if (client == null)
            return false;

        for (Group group : groups) {
            if (group.getClient(client.getId()) != null) {
                group.updateClient(client);
                return true;
            }
        }
        return false;
    }

    public boolean updateGroup(Group group) {
        if (group == null)
            return false;

        for (int i = 0; i < groups.size(); ++i) {
            Group g = groups.get(i);
            if (g == null)
                continue;

            if (group.getId().equals(g.getId())) {
                if (g.equals(group))
                    return false;
                groups.set(i, group);
                return true;
            }
        }
        return false;
    }

    public boolean updateStream(Stream stream) {
        if (stream == null)
            return false;

        for (int i = 0; i < streams.size(); ++i) {
            Stream s = streams.get(i);
            if (s == null)
                continue;

            if (stream.getId().equals(s.getId())) {
                if (s.equals(stream))
                    return false;
                streams.set(i, stream);
                return true;
            }
        }
        streams.add(stream);
        return true;
    }

    public Client getClient(String id) {
        for (Group group: groups) {
            Client client = group.getClient(id);
            if (client != null)
                return client;
        }
        return null;
    }

    public ArrayList<Group> getGroups() {
        return groups;
    }

    public ArrayList<Stream> getStreams() {
        return streams;
    }

    public Stream getStream(String id) {
        for (Stream s : streams)
            if ((s != null) && (s.getId().equals(id)))
                return s;
        return null;
    }

    public Group getGroup(String id) {
        for (Group g : groups)
            if ((g != null) && (g.getId().equals(id)))
                return g;
        return null;
    }

    public JSONArray getJsonStreams() {
        JSONArray jsonArray = new JSONArray();
        for (Stream stream : streams)
            jsonArray.put(stream.toJson());
        return jsonArray;
    }

    public JSONArray getJsonGroups() {
        JSONArray jsonArray = new JSONArray();
        for (Group group : groups)
            jsonArray.put(group.toJson());
        return jsonArray;
    }

}
