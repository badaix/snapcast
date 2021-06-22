
# Stream plugin

A stream plugin is (this might change in future) a binary or script that is started by the server for a specific stream and offers playback control capabilities and provides information about the stream's state, as well as metadata for the currently playing track.
The server communicates via stdin/stdout with the plugin and sends newline delimited JSON-RPC commands and receives responses and notifications from the plugin, as described below. In upcoming releases shared library plugins might be supported as well.  

## Requests:

A Stream plugin must support and handle the following requests, sent by the Snapcast server

### Plugin.Stream.Player.Control:

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.Control", "params": {"command": "SetPosition", "params": { "Position": 170966827, "TrackId": "/org/mpris/MediaPlayer2/Track/2"}}}
```

Supported `command`s: 
- TODO

#### Expected response:

Success:

```json
=> {"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

Error:

```json
todo
```

### Plugin.Stream.Player.SetProperty:

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.SetProperty", "params": {"<name>", <value>}}
```

Supported `propertiey`s: 
- TODO

#### Expected response:

Success:

```json
{"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

Error:

```json
todo
```

### Plugin.Stream.Player.GetProperties

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.GetProperties"}
```

#### Expected response:

Success:

```json
{"id": 1, "jsonrpc": "2.0", "result": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":false,"loopStatus":"none","playbackStatus":"playing","position":593.394,"shuffle":false,"volume":86}}
```

Error:

```json
todo
```

### Plugin.Stream.Player.GetMetadata

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.GetMetadata"}
```

#### Expected response:

Success:

```json
{"id": 1, "jsonrpc": "2.0", "result": {"artist":["Travis Scott & HVME"],"file":"http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3","name":"1Live, Westdeutscher Rundfunk Koeln","title":"Goosebumps (Remix)","trackId":"3"}}
```

## Notifications:

### Plugin.Stream.Player.Metadata

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Metadata", "params": {"artist":["Travis Scott & HVME"],"file":"http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3","name":"1Live, Westdeutscher Rundfunk Koeln","title":"Goosebumps (Remix)","trackId":"3"}}
```

### Plugin.Stream.Player.Properties

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties", "params": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":false,"loopStatus":"none","playbackStatus":"playing","position":593.394,"shuffle":false,"volume":86}}
```

### Plugin.Stream.Log

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Log", "params": {"severity":"Info","message":"Logmessage"}}
```

### Plugin.Stream.Ready

The plugin shall send this notification when it's ready to receive commands

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"}
```


# Server:

## Requests:

To control the stream state, the following commands can be sent to the Snapcast server and will be forwarded to the respective stream:

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.Control", "params": {"id": "Pipe", "command": command, "params": params}}
```

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.SetProperty", "params": {"id": "Pipe", "property": property, "value": value}}
```

## Notifications:

```json
{"jsonrpc": "2.0", "method": "Stream.OnMetadata", "params": {"id": "Pipe", "meta": {}}}
```

```json
{"jsonrpc": "2.0", "method": "Stream.OnProperties", "params": {"id": "Pipe", "properties": {}}}
```
