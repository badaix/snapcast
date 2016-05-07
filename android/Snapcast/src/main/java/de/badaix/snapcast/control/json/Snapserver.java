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
public class Snapserver extends Snapcast {
    int controlProtocolVersion = 1;

    public Snapserver() {
        super();
    }

    public Snapserver(JSONObject json) {
        super(json);
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            super.fromJson(json);
            controlProtocolVersion = json.getInt("controlProtocolVersion");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = super.toJson();
        try {
            json.put("controlProtocolVersion", controlProtocolVersion);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public int getControlProtocolVersion() {
        return controlProtocolVersion;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Snapserver that = (Snapserver) o;

        if (controlProtocolVersion != that.controlProtocolVersion) return false;
        return super.equals(o);
    }

    @Override
    public int hashCode() {
        int result = super.hashCode();
        result = 31 * result + controlProtocolVersion;
        return result;
    }
}

