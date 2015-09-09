import sys
import telnetlib
import json
import threading
import time

telnet = telnetlib.Telnet(sys.argv[1], 1705)

class ReaderThread(threading.Thread):
	def __init__(self, tn, stop_event):
		super(ReaderThread, self).__init__()
		self.tn = tn
		self.stop_event = stop_event

	def run(self):
		while (not self.stop_event.is_set()):
			response = self.tn.read_until("\r\n", 2)
			if response:
				print("received: " + response)
				jresponse = json.loads(response)
				print(json.dumps(jresponse, indent=2))
				print("\r\n")


def doRequest( str, id ):
	print("send: " + str)
	telnet.write(str)
	response = telnet.read_until("\r\n", 2)
	return;


t_stop= threading.Event()
#t = ReaderThread(telnet, t_stop)
#t.start()

volume = sys.argv[2]
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"System.GetStatus\", \"id\": 1}\r\n", 1)
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"00:21:6a:7d:74:fc\", \"volume\": " + volume + "}, \"id\": 2}\r\n", 2)
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"80:1f:02:ed:fd:e0\", \"volume\": " + volume + "}, \"id\": 3}\r\n", 3)
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"74:da:38:00:85:e2\", \"volume\": " + volume + "}, \"id\": 4}\r\n", 4)
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"80:1f:02:ff:79:6e\", \"volume\": " + volume + "}, \"id\": 5}\r\n", 5)
doRequest("{\"jsonrpc\": \"2.0\", \"method\": \"Client.SetVolume\", \"params\": {\"client\": \"bc:5f:f4:ca:cd:64\", \"volume\": " + volume + "}, \"id\": 6}\r\n", 6)

time.sleep(1)
#s = raw_input("")
#print(s)
#t_stop.set();
#t.join()
telnet.close

