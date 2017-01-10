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

import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;


/**
 * Created by johannes on 03.12.16.
 */

public class Group implements JsonSerialisable, Comparable<Group> {
    private String name = "";
    private String id = "";
    private String streamId = "";
    private ArrayList<Client> clients = new ArrayList<Client>();

    public Group(JSONObject json) {
        fromJson(json);
    }

    public Group() {
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            clients.clear();
            name = json.getString("name");
            id = json.getString("id");
            streamId = json.getString("stream");

            JSONArray jClients = json.optJSONArray("clients");
            if (jClients != null) {
                for (int i = 0; i < jClients.length(); i++)
                    clients.add(new Client(jClients.getJSONObject(i)));
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
        sort();
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("name", name);
            json.put("id", id);
            json.put("stream", streamId);
            json.put("clients", getJsonClients());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public void sort() {
        Collections.sort(clients);
    }

    public String getName() {

        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public String getStreamId() {
        return streamId;
    }

    public void setStreamId(String streamId) {
        this.streamId = streamId;
    }

    public ArrayList<Client> getClients() {
        return clients;
    }


    public boolean removeClient(String id) {
        if (TextUtils.isEmpty(id))
            return false;

        for (int i = 0; i < clients.size(); ++i) {
            if (clients.get(i).getId().equals(id)) {
                clients.remove(i);
                return true;
            }
        }
        return false;
    }


    boolean updateClient(Client client) {
        if (client == null)
            return false;

        for (int i = 0; i < clients.size(); ++i) {
            Client c = clients.get(i);
            if (c == null)
                continue;

            if (client.getId().equals(c.getId())) {
                if (client.equals(c))
                    return true;
                clients.set(i, client);
                return true;
            }
        }
        return false;
    }


    public Client getClient(String id) {
        if (TextUtils.isEmpty(id))
            return null;

        for (Client c: clients) {
            if (c == null)
                continue;

            if (id.equals(c.getId()))
                return c;
        }
        return null;
    }

    public JSONArray getJsonClients() {
        JSONArray jsonArray = new JSONArray();
        for (Client client : clients)
            jsonArray.put(client.toJson());
        return jsonArray;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Group group = (Group) o;

        if (name != null ? !name.equals(group.name) : group.name != null) return false;
        if (id != null ? !id.equals(group.id) : group.id != null) return false;
        if (streamId != null ? !streamId.equals(group.streamId) : group.streamId != null)
            return false;
        return clients != null ? clients.equals(group.clients) : group.clients == null;

    }

    @Override
    public int hashCode() {
        int result = name != null ? name.hashCode() : 0;
        result = 31 * result + (id != null ? id.hashCode() : 0);
        result = 31 * result + (streamId != null ? streamId.hashCode() : 0);
        result = 31 * result + (clients != null ? clients.hashCode() : 0);
        return result;
    }

    @Override
    public int compareTo(Group another) {
        if (getClients().isEmpty())
            return -1;
        if (another.getClients().isEmpty())
            return 1;
        return getClients().get(0).compareTo(another.getClients().get(0));
    }
}
