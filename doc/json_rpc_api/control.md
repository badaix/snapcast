# Snapcast JSON RPC Control API

## Raw TCP sockets

Snapcast can be controlled with a [JSON-RPC 2.0](http://www.jsonrpc.org/specification)
API over a raw TCP-Socket interface on port 1705.

Single JSON Messages are new line delimited ([ndjson](http://ndjson.org/)).

For simple tests you can fire JSON commands on a telnet connection and watch
Notifications coming in:

```json
$ telnet localhost 1705
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
// Call "Server.GetRPCVersion"
{"id":8,"jsonrpc":"2.0","method":"Server.GetRPCVersion"}
// Response is:
{"id":8,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
// Connect a client
{"jsonrpc":"2.0","method":"Client.OnConnect","params":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488065507,"usec":820050},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.11.0-beta-1"}},"id":"00:21:6a:7d:74:fc"}}
```

## HTTP

Snapcast can also be controlled on port 1780 using the HTTP protocol to (1) send
a single `POST` request or (2) create a long-lived `WebSocket`.

For simple tests, you can fire JSON commands directly within your browser.
One-shot `POST` requests receive only the immediate response of the request,
whereas long-lived `WebSocket`s will also receive Notifications similar to the
telnet connection above:

```js

const host = '127.0.0.1:1780';
const request = {
    'id': 0,
    'jsonrpc': '2.0',
    'method': 'Server.GetRPCVersion'
};

// XHR
const xhr = new XMLHttpRequest();
xhr.open('POST', `http://${host}/jsonrpc`);
xhr.setRequestHeader('Content-Type', 'application/json');
xhr.setRequestHeader('Accept', 'application/json');
xhr.addEventListener('load', ({ currentTarget: xhr }) => {
    console.log(JSON.parse(xhr.responseText)); // {"id":1,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
});
xhr.send(JSON.stringify(++request.id && request));

// Fetch
fetch(`http://${host}/jsonrpc`, {
    method: 'POST',
    headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    },
    body: JSON.stringify(++request.id && request)
})
    .then(response => response.json())
    .then(content => console.log(content)); // {"id":2,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}

// Fetch with await/async
const response = await fetch(`http://${host}/jsonrpc`, {
    method: 'POST',
    headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    },
    body: JSON.stringify(++request.id && request)
});
const content = await response.json();
console.log(content);  // {"id":3,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}


// WebSocket
const ws = new WebSocket(`ws://${host}/jsonrpc`);
ws.addEventListener('message', (message) => {
    console.log(JSON.parse(message.data)); // {"id":4,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
});
ws.addEventListener('open', () => ws.send(JSON.stringify(++request.id && request)));

/*
WebSockets receive Notifications of events. Connect a client, and you will eventually see in your console:

{
  "jsonrpc": "2.0",
  "method": "Client.OnConnect",
  "params": { ... }
}
*/
```

## Requests and Notifications

The client that sends a "Set" command will receive a Response, while the other connected control clients will receive a Notification "On" event.
Commands can be sent in a [Batch](http://www.jsonrpc.org/specification#batch). The server will reply with a Batch and send a Batch notification to the other clients. This way the volume of multiple Snapclients can be changed with a single Batch Request.

Each JSON RPC request and response contains an identifier in the `id` field, which can be used to link responses to the request that caused them, as defined in the JSON RPC specification:

> An identifier established by the Client that MUST contain a String, Number, or NULL value if included.
> If it is not included it is assumed to be a notification. The value SHOULD normally not be Null [1] and Numbers SHOULD NOT contain fractional parts [2]

Clients should call `Server.GetStatus` to get the complete picture.

The Server JSON object contains a list of Groups and Streams. Every Group holds a list of Clients and a reference to a Stream. Clients, Groups and Streams are referenced in the "Set" commands by their `id`.

### Example JSON objects

#### Client

```json
{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026416,"usec":135973},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}
```

#### Volume

```json
{"muted":false,"percent":74}
```

#### Group

```json
{"clients":[{"config":{"instance":2,"latency":10,"name":"Laptop","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488026485,"usec":644997},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026481,"usec":223747},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":true,"name":"","stream_id":"stream 1"}
```

#### Stream

```json
{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}}
```

#### Server

```json
{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}
```

### Requests

* Client
  * [Client.GetStatus](#clientgetstatus)
  * [Client.SetVolume](#clientsetvolume)
  * [Client.SetLatency](#clientsetlatency)
  * [Client.SetName](#clientsetname)
* Group
  * [Group.GetStatus](#groupgetstatus)
  * [Group.SetMute](#groupsetmute)
  * [Group.SetStream](#groupsetstream)
  * [Group.SetClients](#groupsetclients)
  * [Group.SetName](#groupsetname)
* Server
  * [Server.GetRPCVersion](#servergetrpcversion)
  * [Server.GetStatus](#servergetstatus)
  * [Server.DeleteClient](#serverdeleteclient)
* Stream
  * [Stream.AddStream](#streamaddstream)
  * [Stream.RemoveStream](#streamremovestream)
  * [Stream.Control](#streamcontrol)
  * [Stream.SetProperty](#streamsetproperty)

### Notifications

* Client
  * [Client.OnConnect](#clientonconnect)
  * [Client.OnDisconnect](#clientondisconnect)
  * [Client.OnVolumeChanged](#clientonvolumechanged)
  * [Client.OnLatencyChanged](#clientonlatencychanged)
  * [Client.OnNameChanged](#clientonnamechanged)
* Group
  * [Group.OnMute](#grouponmute)
  * [Group.OnStreamChanged](#grouponstreamchanged)
  * [Group.OnNameChanged](#grouponnamechanged)
* Stream
  * [Stream.OnProperties](#streamonproperties)
  * [Stream.OnUpdate](#streamonupdate)
* Server
  * [Server.OnUpdate](#serveronupdate)

## Requests

Any request might be answered with a generic [json-rpc-2.0 error](https://www.jsonrpc.org/specification#error_object):

```json
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32600, "message": "Invalid request"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32601, "message": "Method not found"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Invalid params"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32603, "message": "Internal error"}}
```

Some requests might return more specific json error messages.

### Client.GetStatus

#### Request

```json
{"id":8,"jsonrpc":"2.0","method":"Client.GetStatus","params":{"id":"00:21:6a:7d:74:fc"}}
```

#### Response

```json
{"id":8,"jsonrpc":"2.0","result":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026416,"usec":135973},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}}}
```

### Client.SetVolume

#### Request

```json
{"id":"8","jsonrpc":"2.0","method":"Client.SetVolume","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
```

#### Response

```json
{"id":"8","jsonrpc":"2.0","result":{"volume":{"muted":false,"percent":74}}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Client.OnVolumeChanged","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
```

### Client.SetLatency

#### Request

```json
{"id":7,"jsonrpc":"2.0","method":"Client.SetLatency","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
```

#### Response

```json
{"id":7,"jsonrpc":"2.0","result":{"latency":10}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Client.OnLatencyChanged","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
```

### Client.SetName

#### Request

```json
{"id":6,"jsonrpc":"2.0","method":"Client.SetName","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
```

#### Response

```json
{"id":6,"jsonrpc":"2.0","result":{"name":"Laptop"}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Client.OnNameChanged","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
```

### Group.GetStatus

#### Request

```json
{"id":5,"jsonrpc":"2.0","method":"Group.GetStatus","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
```

#### Response

```json
{"id":5,"jsonrpc":"2.0","result":{"group":{"clients":[{"config":{"instance":2,"latency":10,"name":"Laptop","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488026485,"usec":644997},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026481,"usec":223747},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":true,"name":"","stream_id":"stream 1"}}}
```

### Group.SetMute

#### Request

```json
{"id":5,"jsonrpc":"2.0","method":"Group.SetMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
```

#### Response

```json
{"id":5,"jsonrpc":"2.0","result":{"mute":true}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Group.OnMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
```

### Group.SetStream

#### Request

```json
{"id":4,"jsonrpc":"2.0","method":"Group.SetStream","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
```

#### Response

```json
{"id":4,"jsonrpc":"2.0","result":{"stream_id":"stream 1"}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Group.OnStreamChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
```

### Group.SetClients

#### Request

```json
{"id":3,"jsonrpc":"2.0","method":"Group.SetClients","params":{"clients":["00:21:6a:7d:74:fc#2","00:21:6a:7d:74:fc"],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
```

#### Response

```json
{"id":3,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```

### Group.SetName

#### Request

```json
{"id":7,"jsonrpc":"2.0","method":"Group.SetName","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","name":"GroundFloor"}}
```

#### Response

```json
{"id":7,"jsonrpc":"2.0","result":{"name":"GroundFloor"}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Group.OnNameChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","name":"GroundFloor"}}
```

### Server.GetRPCVersion

#### Request

```json
{"id":8,"jsonrpc":"2.0","method":"Server.GetRPCVersion"}
```

#### Response

```json
{"id":8,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
```

### Server.GetStatus

#### Request

```json
{"id":1,"jsonrpc":"2.0","method":"Server.GetStatus"}
```

#### Response

```json
{"id":1,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025696,"usec":578142},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":true,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025696,"usec":611255},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```

### Server.DeleteClient

#### Request

```json
{"id":2,"jsonrpc":"2.0","method":"Server.DeleteClient","params":{"id":"00:21:6a:7d:74:fc"}}
```

#### Response

```json
{"id":2,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```

#### Notification

```json
{"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```

### Stream.AddStream

#### Request

```json
{"id":8,"jsonrpc":"2.0","method":"Stream.AddStream","params":{"streamUri":"pipe:///tmp/snapfifo?name=stream 2"}}
```

#### Response

```json
{"id":8,"jsonrpc":"2.0","result":{"stream_id":"stream 2"}}
```

### Stream.RemoveStream

#### Request

```json
{"id":8,"jsonrpc":"2.0","method":"Stream.RemoveStream","params":{"id":"stream 2"}}
```

#### Response

```json
{"id":8,"jsonrpc":"2.0","result":{"stream_id":"stream 2"}}
```

### Stream.Control

#### Request

Same request as in [Plugin.Stream.Player.Control](stream_plugin.md#pluginstreamplayercontrol), but with the method `Stream.Control`.

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.Control", "params": {"command": "<command>", "params": { "<param 1>": <value 1>, "<param 2>": <value 2>}}}
```

Example:

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.Control", "params": {"id": "Spotify", "command": "next", params: {}}}
{"id": 1, "jsonrpc": "2.0", "method": "Stream.Control", "params": {"id": "Spotify", "command": "seek", "param": "60"}}
```

#### Supported `command`s

See [Plugin.Stream.Player.Control](stream_plugin.md#pluginstreamplayercontrol).

#### Response

##### Success

```json
{"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

##### Error

```json
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32603, "message": "Stream not found"}}

{"id": 1, "jsonrpc": "2.0", "error": {"code": 1, "message": "Stream can not be controlled"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 2, "message": "Stream property canGoNext is false"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 3, "message": "Stream property canGoPrevious is false"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 4, "message": "Stream property canPlay is false"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 5, "message": "Stream property canPause is false"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 6, "message": "Stream property canSeek is false"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": 7, "message": "Stream property canControl is false"}}

{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Command '<command>' not supported"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Parameter 'commmand' is missing"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "setPosition requires parameter 'position'"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "seek requires parameter 'offset'"}}
```

### Stream.SetProperty

#### Request

Same request as in [Plugin.Stream.Player.SetProperty](stream_plugin.md#pluginstreamplayersetproperty), but with the method `Stream.SetProperty`.

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.SetProperty", "params": {"id": "Pipe", "property": property, "value": value}}
```

#### Supported `property`s

See [Plugin.Stream.Player.SetProperty](stream_plugin.md#pluginstreamplayersetproperty).

#### Response

##### Success

```json
{"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

##### Error

```json
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Property '<property>' not supported"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Parameter 'property' is missing"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Parameter 'value' is missing"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Value for loopStatus must be one of 'none', 'track', 'playlist'"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Value for shuffle must be bool"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Value for volume must be an int"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Value for mute must be bool"}}
{"id": 1, "jsonrpc": "2.0", "error": {"code": -32602, "message": "Value for rate must be float"}}
```

## Notifications

### Client.OnConnect

```json
{"jsonrpc":"2.0","method":"Client.OnConnect","params":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":true,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025524,"usec":876332},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},"id":"00:21:6a:7d:74:fc"}}
```

### Client.OnDisconnect

```json
{"jsonrpc":"2.0","method":"Client.OnDisconnect","params":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":false,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025523,"usec":814067},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},"id":"00:21:6a:7d:74:fc"}}
```

### Client.OnVolumeChanged

```json
{"jsonrpc":"2.0","method":"Client.OnVolumeChanged","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
```

### Client.OnLatencyChanged

```json
{"jsonrpc":"2.0","method":"Client.OnLatencyChanged","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
```

### Client.OnNameChanged

```json
{"jsonrpc":"2.0","method":"Client.OnNameChanged","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
```

### Group.OnMute

```json
{"jsonrpc":"2.0","method":"Group.OnMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
```

### Group.OnStreamChanged

```json
{"jsonrpc":"2.0","method":"Group.OnStreamChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
```

### Group.OnNameChanged

```json
{"jsonrpc":"2.0","method":"Group.OnNameChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","name":"GroundFloor"}}
```

### Stream.OnUpdate

```json
{"jsonrpc":"2.0","method":"Stream.OnUpdate","params":{"id":"stream 1","stream":{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}}}}
```

### Stream.OnProperties

Same notification as in [Plugin.Stream.Player.Properties](stream_plugin.md#pluginstreamplayerproperties), but with the method `Stream.OnProperties`.

```json
{"jsonrpc":"2.0","method":"Stream.OnProperties","params":{"id":"stream 1", "metadata": {"album": "some album", "artist": "some artist", "track": "some track"...}}}
```

### Server.OnUpdate

```json
{"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
```
