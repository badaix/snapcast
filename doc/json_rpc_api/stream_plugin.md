
# Stream plugin

A stream plugin is (this might change in future) an executable binary or script that is started by the server for a specific stream and offers playback control capabilities and provides information about the stream's state, as well as metadata for the currently playing track.
The Snapcast server communicates via stdin/stdout with the plugin and sends newline delimited JSON-RPC commands and receives responses and notifications from the plugin, as described below. In upcoming releases shared library plugins might be supported as well.  

## Requests:

A Stream plugin must support and handle the following requests, sent by the Snapcast server

### Plugin.Stream.Player.Control:

Used to control the player

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.Control", "params": {"command": "<command>", "params": { "<param 1>": <value 1>, "<param 2>": <value 2>}}}
```

Supported `command`s:

- `Play`: Start playback
    - `params`: none
- `Pause`: Stop playback
    - `params`: none
- `PlayPause`: Toggle play/pause
    - `params`: none
- `Stop`: Stop playback
    - `params`: none
- `Next`: Skip to next track
    - `params`: none
- `Previous`: Skip to previous track
    - `params`: none
- `Seek`: Seek forward or backward in the current track
    - `params`:
        - `Offset`: [float] seek offset in seconds
- `SetPosition`: Set the current track position in seconds
    - `params`:
        - `Position`: [float] the new track position
        - `TrackId`: [string] the optional currently playing track's identifier 

#### Example: 

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.Control", "params": {"command": "SetPosition", "params": { "Position": 170966827, "TrackId": "/org/mpris/MediaPlayer2/Track/2"}}}
```


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
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.SetProperty", "params": {"<property>", <value>}}
```

#### Supported `property`s: 

- `loopStatus`: [string] the current repeat status, one of:
    - `none`: the playback will stop when there are no more tracks to play
    - `track`: the current track will start again from the begining once it has finished playing
    - `playlist`: the playback loops through a list of tracks
- `shuffle`: [bool] play playlist in random order
- `volume`: [int] voume in percent, valid range [0..100]
- `rate`: [float] The current playback rate, valid range (0..)

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

#### Supported `property`s

- `playbackStatus`: [string] The current playback status, one of:
    - `playing`
    - `paused` 
    - `stopped` 
- `loopStatus`: [string] The current repeat status, one of:
    - `none`: The playback will stop when there are no more tracks to play
    - `track`: The current track will start again from the begining once it has finished playing
    - `playlist`: The playback loops through a list of tracks
- `rate`: [float] The current playback rate, valid range (0..)
- `shuffle`: [bool] Traverse through the playlist in random order
- `volume`: [int] Voume in percent, valid range [0..100]
- `position`
- `canGoNext`: [bool] Whether the client can call the Next method on this interface and expect the current track to change
- `canGoPrevious`: [bool] Whether the client can call the Previous method on this interface and expect the current track to change
- `canPlay`: [bool] Whether playback can be started using Play or PlayPause
- `canPause`: [bool] Whether playback can be paused using Pause or PlayPause
- `canSeek`: [bool] Whether the client can control the playback position using Seek and SetPosition
- `canControl`: [bool] Whether the media player may be controlled over this interface

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

#### Supported `tag`s

- `trackId`: [string] A unique identity for this track within the context of an MPRIS object (eg: tracklist).
- `file`: [string] The current song.
- `duration`: [float] The duration of the song in seconds; may contain a fractional part.
- `artist`: [list of strings] The track artist(s).
- `artistSort`: [list of strings] Same as artist, but for sorting. This usually omits prefixes such as “The”.
- `album`: [string] The album name.
- `albumSort`: [string] Same as album, but for sorting.
- `albumArtist`: [list of strings] The album artist(s).
- `albumArtistSort`: [list of strings] Same as albumartist, but for sorting.
- `name`: [string] A name for this song. This is not the song title. The exact meaning of this tag is not well-defined. It is often used by badly configured internet radio stations with broken tags to squeeze both the artist name and the song title in one tag.
- `date`: [string] The song’s release date. This is usually a 4-digit year.
- `originalDate`: [string] The song’s original release date.
- `composer`: [list of strings] The composer(s) of the track.
- `performer`: [string] The artist who performed the song.
- `conductor`: [string] The conductor who conducted the song.
- `work`: [string] “a work is a distinct intellectual or artistic creation, which can be expressed in the form of one or more audio recordings”
- `grouping`: [string] “used if the sound belongs to a larger category of sounds/music” (from the IDv2.4.0 TIT1 description).
- `comment`: [list of string] A (list of) freeform comment(s) 
- `label`: [string] The name of the label or publisher.
- `musicbrainzArtistId`: [string] The artist id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `musicbrainzAlbumId`: [string] The album id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `musicbrainzAlbumArtistId`: [string] The album artist id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `musicbrainzTrackId`: [string] The track id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `musicbrainzReleaseTrackId`: [string] The release track id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `musicbrainzWorkId`: [string] The work id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
- `lyrics`: [list of strings] The lyricist(s) of the track
- `bpm`: [integer] The speed of the music, in beats per minute.
- `autoRating`: [float] An automatically-generated rating, based on things such as how often it has been played. This should be in the range 0.0 to 1.0.
- `comment`: [list of strings] A (list of) freeform comment(s).
- `composer`: [list of strings] The composer(s) of the track.
- `contentCreated`: [string] Date/Time: When the track was created. Usually only the year component will be useful.
- `discNumber`: [integer] The disc number on the album that this track is from.
- `firstUsed`: [string] Date/Time: When the track was first played.
- `genre`: [list of strings] List of Strings: The genre(s) of the track.
- `lastUsed`: [string] Date/Time: When the track was last played.
- `lyricist`: [list of strings] List of Strings: The lyricist(s) of the track.
- `title`: [string] The track title.
- `trackNumber`: [string] The track number on the album disc.
- `url`: [string uri] The location of the media file.
- `artUrl`: [string uri] The location of an image representing the track or album. Clients should not assume this will continue to exist when the media player stops giving out the URL.
- `useCount`: [integer] The number of times the track has been played.
- `userRating`: [float] A user-specified rating. This should be in the range 0.0 to 1.0. 
- `spotifyArtistId`: [string] The [Spotify Artist ID](https://developer.spotify.com/documentation/web-api/#spotify-uris-and-ids)
- `spotifyTrackId`: [string] The [Spotify Track ID](https://developer.spotify.com/documentation/web-api/#spotify-uris-and-ids)


### Plugin.Stream.Player.Properties

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties", "params": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":false,"loopStatus":"none","playbackStatus":"playing","position":593.394,"shuffle":false,"volume":86}}
```

[Supported `property`s](#supported-propertys)

### Plugin.Stream.Log

Send a log message

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Log", "params": {"severity":"Info","message":"<Log message>"}}
```

#### Supported `severity`s

- `trace` verbose debug-level message
- `debug` debug-level message
- `info` informational message
- `notice` normal, but significant, condition
- `warning` warning conditions
- `error` error conditions
- `fatal` critical conditions

### Plugin.Stream.Ready

The plugin shall send this notification when it's up and ready to receive commands

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"}
```


# Server:

TODO: this belongs to doc/json_rpc_api/v2_0_0.md

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
{"jsonrpc": "2.0", "method": "Stream.OnMetadata", "params": {"id": "Pipe", "metadata": {}}}
```

```json
{"jsonrpc": "2.0", "method": "Stream.OnProperties", "params": {"id": "Pipe", "properties": {}}}
```
