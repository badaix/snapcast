#!/usr/bin/env python
import sys
import telnetlib
import json
import threading
import time

if len(sys.argv) < 3:
	print("usage: control.py <SERVER HOST> [setVolume|setName]")
	sys.exit(0)

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
				return jResponse;
	return;

def setVolume(client, volume):
	global requestId
	doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Client.SetVolume', 'params': {'client':  client, 'volume': volume}, 'id': requestId}), requestId)
	requestId = requestId + 1

def setName(client, name):
	global requestId
	doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Client.SetName', 'params': {'client':  client, 'name': name}, 'id': requestId}), requestId)
	requestId = requestId + 1

if sys.argv[2] == "setVolume":
	if len(sys.argv) < 5:
		print("usage: control.py <SERVER HOST> setVolume <HOSTNAME> [+/-]<VOLUME>")
		exit(0)
	volstr = sys.argv[4]
	j = doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Server.GetStatus', 'id': 1}), 1)
	for client in j["result"]["clients"]:
		if(sys.argv[3] == client['host']['name'] or sys.argv[3] == 'all'):
			if(volstr[0] == '+'):
				volume = int(client['config']['volume']['percent']) + int(volstr[1:])
			elif(volstr[0] == '-'):
				volume = int(client['config']['volume']['percent']) - int(volstr[1:])
			else:
				volume = int(volstr)
			setVolume(client['host']['mac'], volume)

elif sys.argv[2] == "setName":
	if len(sys.argv) < 5:
		print("usage: control.py <SERVER HOST> setName <MAC> <NAME>")
		exit(0)
	setName(sys.argv[3], sys.argv[4])

else:
	print("unknown command \"" + sys.argv[2] + "\"")

j = doRequest(json.dumps({'jsonrpc': '2.0', 'method': 'Server.GetStatus', 'id': 1}), 1)
for client in j["result"]["clients"]:
	print("MAC: " + client['host']['mac'] + ", connect: " + str(client['connected']) + ", volume: " + str(client['config']['volume']['percent']) + ", name: " + client['host']['name'] + ", host: " + client['host']['ip'])

telnet.close

