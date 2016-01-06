package de.badaix.snapcast.control;

import java.util.Vector;

/**
 * Created by johannes on 06.01.16.
 */
public class ServerInfo {
    private Vector<ClientInfo> clientInfos = new Vector<ClientInfo>();

    public ServerInfo() {

    }

    public boolean addClient(ClientInfo client) {
        if (client == null)
            return false;

        for (ClientInfo clientInfo: clientInfos) {
            if (client.getMac().equals(clientInfo.getMac())) {
                if (clientInfo.equals(client))
                    return false;
                clientInfos.add(client);
                return true;
            }
        }
        clientInfos.add(client);
        return true;
    }

    public Vector<ClientInfo> getClientInfos() {
        return clientInfos;
    }
}
