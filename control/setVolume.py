#!/usr/bin/env python
import sys
import telnetlib
import json

telnet = telnetlib.Telnet(sys.argv[1], 1705)
requestId = 1


def doRequest(j, requestId):
    print("send: " + j)
    telnet.write(j + "\r\n")
    while (True):
        response = telnet.read_until("\r\n", 2)
        jResponse = json.loads(response)
        if 'id' in jResponse:
            if jResponse['id'] == requestId:
                # print("recv: " + response)
                return jResponse


def setVolume(client, volume):
    global requestId
    doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Client.SetVolume', 'params': {
              'id':  client, 'volume': {'muted': False, 'percent': volume}}, 'id': requestId}), requestId)
    requestId = requestId + 1


volume = int(sys.argv[2])
j = doRequest(json.dumps(
    {'jsonrpc': '2.0', 'method': 'Server.GetStatus', 'id': 1}), 1)
for group in j["result"]["server"]["groups"]:
    for client in group["clients"]:
        setVolume(client['id'], volume)

j = doRequest(json.dumps(
    {'jsonrpc': '2.0', 'method': 'Server.GetStatus', 'id': 1}), 1)
for group in j["result"]["server"]["groups"]:
    for client in group["clients"]:
        print("MAC: " + client['host']['mac'] + ", name: " + client['config']['name'] + ", conntect: " + str(
            client['connected']) + ", volume: " + str(client['config']['volume']['percent']))

telnet.close
