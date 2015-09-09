import sys
import telnetlib
import json
import threading
import time

telnet = telnetlib.Telnet(sys.argv[1], 1705)
requestId = 1

def doRequest( j, requestId ):
	print("send: " + j)
	telnet.write(j + "\r\n")
	while (True):
		response = telnet.read_until("\r\n", 2)
		jResponse = json.loads(response)
		if 'id' in jResponse:
			if jResponse['id'] == requestId:
				print("recv: " + response)
				return;
	return;

def setVolume(client, volume):
	global requestId
	doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Client.SetVolume', 'params': {'client':  client, 'volume': volume}, 'id': requestId}), requestId)
	requestId = requestId + 1

volume = int(sys.argv[2])
doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'System.GetStatus', 'id': 1}), 1)
setVolume("00:21:6a:7d:74:fc", volume)
setVolume("80:1f:02:ed:fd:e0", volume)
setVolume("74:da:38:00:85:e2", volume)
setVolume("80:1f:02:ff:79:6e", volume)
setVolume("bc:5f:f4:ca:cd:64", volume)
telnet.close

