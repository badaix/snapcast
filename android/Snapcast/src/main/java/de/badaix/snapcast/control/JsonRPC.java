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

package de.badaix.snapcast.control;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 19.02.17.
 */

class RPCRequest {
    RPCRequest(JSONObject json) throws JSONException {
        fromJson(json);
    }

    RPCRequest(String json) throws JSONException {
        this(new JSONObject(json));
    }

    RPCRequest(String method, long id, JSONObject params) {
        this.method = method;
        this.id = id;
        this.params = params;
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    JSONObject toJson() {
        JSONObject request = new JSONObject();
        try {
            request.put("jsonrpc", "2.0");
            request.put("method", method);
            request.put("id", id);
            if (params != null)
                request.put("params", params);
            return request;
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }
    }

    void fromJson(JSONObject json) throws JSONException {
        id = json.getLong("id");
        method = json.getString("method");
        if (json.has("params"))
            params = json.getJSONObject("params");
        else
            params = null;
    }

    String method;
    long id;
    JSONObject params;
}


class RPCNotification {
    RPCNotification(JSONObject json) throws JSONException {
        fromJson(json);
    }

    RPCNotification(String json) throws JSONException {
        this(new JSONObject(json));
    }

    RPCNotification(String method, long id, JSONObject params) {
        this.method = method;
        this.params = params;
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    JSONObject toJson() {
        JSONObject request = new JSONObject();
        try {
            request.put("jsonrpc", "2.0");
            request.put("method", method);
            if (params != null)
                request.put("params", params);
            return request;
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }
    }

    void fromJson(JSONObject json) throws JSONException {
        method = json.getString("method");
        if (json.has("params"))
            params = json.getJSONObject("params");
        else
            params = null;
    }

    String method;
    JSONObject params;
}


class RPCResponse {
    RPCResponse(JSONObject json) throws JSONException {
        fromJson(json);
    }

    RPCResponse(String json) throws JSONException {
        this(new JSONObject(json));
    }

    @Override
    public String toString() {
        return toJson().toString();
    }

    JSONObject toJson() {
        JSONObject response = new JSONObject();
        try {
            response.put("jsonrpc", "2.0");
            if (error != null)
                response.put("error", error);
            else if (result != null)
                response.put("result", result);
            else
                throw new JSONException("error and result are null");

            response.put("id", id);
            return response;
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }
    }

    void fromJson(JSONObject json) throws JSONException {
        id = json.getLong("id");
        if (json.has("error")) {
            error = json.getJSONObject("error");
            result = null;
        }
        else if (json.has("result")) {
            result = json.getJSONObject("result");
            error = null;
        }
        else
            throw new JSONException("error and result are null");
    }

    long id;
    JSONObject result;
    JSONObject error;
}

