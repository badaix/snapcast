import sys
import telnetlib
import json
import threading
import time

HOST = "localhost"

telnet = telnetlib.Telnet(HOST, 1705)


class ReaderThread(threading.Thread):
	def __init__(self, tn, stop_event):
		super(ReaderThread, self).__init__()
		self.tn = tn
		self.stop_event = stop_event

	def run(self):
		while (not self.stop_event.is_set()):
			response = self.tn.read_until("\r\n", 1)
			if response:
				jresponse = json.loads(response)
				print(json.dumps(jresponse, indent=2))
				print("\r\n")


def doRequest( str ):
	print(str)
	telnet.write(str)
	time.sleep(1)
	return;


t_stop= threading.Event()
t = ReaderThread(telnet, t_stop)
t.start()

doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\"}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"volume\": 83}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetLatency\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"latency\": 10}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetName\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"name\": \"living room\"}, \"id\": 2}\r\n")
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetMute\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"mute\": false}, \"id\": 2}\r\n")

s = raw_input("")
print(s)
t_stop.set();
t.join()
telnet.close
