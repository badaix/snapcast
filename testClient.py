import sys
import telnetlib
import json

HOST = "localhost"

tn = telnetlib.Telnet(HOST, 1705)

def doRequest( str ):
	tn.write(str)
	response = json.loads(tn.read_until("\r\n"))
	print(json.dumps(response, indent=2))
	return;

doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\"}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"volume\": 83}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetLatency\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"latency\": 10}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetName\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"name\": \"living room\"}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetMute\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"mute\": false}, \"id\": 2}\r\n")


tn.close
