package de.badaix.snapcast.control;

import java.util.Vector;

/**
 * Created by johannes on 06.01.16.
 */
public class ServerInfo {
    private Vector<ClientInfo> clientInfos = new Vector<ClientInfo>();

    public ServerInfo() {

    }

    public void clear() {
        clientInfos.clear();
    }

    public boolean removeClient(ClientInfo client) {
        for (int i=0; i<clientInfos.size(); ++i) {
            if (clientInfos.get(i).getMac().equals(client.getMac())) {
                clientInfos.remove(i);
                return true;
            }
        }
        return false;
    }

    public boolean addClient(ClientInfo client) {
        if (client == null)
            return false;

        for (int i=0; i<clientInfos.size(); ++i) {
            ClientInfo clientInfo = clientInfos.get(i);
            if (clientInfo == null)
                continue;

            if (client.getMac().equals(clientInfo.getMac())) {
                if (clientInfo.equals(client))
                    return false;
                clientInfos.set(i, client);
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
