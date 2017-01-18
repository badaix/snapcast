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
public class Time_t implements JsonSerialisable {
    private long sec = 0;
    private long usec = 0;

    public Time_t() {

    }

    public Time_t(JSONObject json) {
        fromJson(json);
    }

    public Time_t(long sec, long usec) {
        this.sec = sec;
        this.usec = usec;
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            sec = json.getLong("sec");
            usec = json.getLong("usec");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("sec", sec);
            json.put("usec", usec);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public long getSec() {
        return sec;
    }

    public void setSec(long sec) {
        this.sec = sec;
    }

    public long getUsec() {
        return usec;
    }

    public void setUsec(long usec) {
        this.usec = usec;
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Time_t time_t = (Time_t) o;

        if (sec != time_t.sec) return false;
        return usec == time_t.usec;

    }

    @Override
    public int hashCode() {
        int result = (int) sec;
        result = (int) (31 * result + usec);
        return result;
    }
}
