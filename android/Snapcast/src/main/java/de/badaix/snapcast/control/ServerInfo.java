package de.badaix.snapcast.control;

import java.util.ArrayList;

/**
 * Created by johannes on 06.01.16.
 */
public class ServerInfo {
    private ArrayList<ClientInfo> clientInfos = new ArrayList<ClientInfo>();
    private ArrayList<Stream> streams = new ArrayList<Stream>();

    public ServerInfo() {

    }

    public void clear() {
        clientInfos.clear();
    }

    public boolean removeClient(ClientInfo client) {
        for (int i = 0; i < clientInfos.size(); ++i) {
            if (clientInfos.get(i).getMac().equals(client.getMac())) {
                clientInfos.remove(i);
                return true;
            }
        }
        return false;
    }

    public boolean updateClient(ClientInfo client) {
        if (client == null)
            return false;

        for (int i = 0; i < clientInfos.size(); ++i) {
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

    public boolean updateStream(Stream stream) {
        if (stream == null)
            return false;

        for (int i = 0; i < streams.size(); ++i) {
            Stream s = streams.get(i);
            if (s == null)
                continue;


            if (stream.getUri().equals(s.getUri())) {
                if (stream.equals(stream))
                    return false;
                streams.set(i, stream);
                return true;
            }
        }
        streams.add(stream);
        return true;
    }

    public ArrayList<ClientInfo> getClientInfos() {
        return clientInfos;
    }

    public ArrayList<Stream> getStreams() {
        return streams;
    }
}
