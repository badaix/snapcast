
# Stream plugin

A stream plugin is an executable binary or script that is started by the server for a specific stream and offers playback control capabilities and provides information about the stream's state, as well as metadata for the currently playing track.
The Snapcast server communicates via stdin/stdout with the plugin and sends [newline delimited](http://ndjson.org/) [JSON-RPC-2.0](https://www.jsonrpc.org/specification) commands and receives responses and notifications from the plugin, as described below. In upcoming releases shared library plugins might be supported as well.  
A stream is connected to a plugin with the `controlscript` stream source parameter, e.g.

```ini
[stream]
source = pipe:///tmp/snapfifo?name=MPD&controlscript=meta_mpd.py
```

If a relative path is given, Snapserver will search the script in `/usr/share/snapserver/plug-ins`. Example scripts `meta_mpd.py` and `meta_mopidy.py` are shipped with the server installation, that can be used with mpd and mopidy stream sources.
Paramters are passed to the control script using the `controlscriptparams` stream source parameter. Snapserver will always pass the parameter `--stream=<id of the stream>` and, if HTTP is enabled, `--snapcast-port=<http.port>` and `--snapcast-host=<http.host>`

A Stream plugin must support and handle the following [requests](#requests), sent by the Snapcast server

* [Plugin.Stream.Player.Control](#pluginstreamplayercontrol)
* [Plugin.Stream.Player.SetProperty](#pluginstreamplayersetproperty)
* [Plugin.Stream.Player.GetProperties](#pluginstreamplayergetproperties)

The following [notifications](#notifications) can be sent by the plugin to notify the server about changes. The `Plugin.Stream.Ready` should be fired as soon as the plugin is ready to receive commands, upon reception, the server will query the stream's [properties](#pluginstreamplayergetproperties)

* [Plugin.Stream.Player.Properties](#pluginstreamplayerproperties)
* [Plugin.Stream.Log](#pluginstreamlog)
* [Plugin.Stream.Ready](#pluginstreamready)

## Requests

The following requests will be sent by the Snapserver to the stream plugin. Upon startup Snapserver will wait for the plugin's [ready notification](#pluginstreamready) to [query the supported properties](Plugin.Stream.Player.GetProperties). The controlling capabilities are encoded into these boolean properties: `canGoNext`, `canGoPrevious`, `canPlay`, `canPause`, `canSeek`, `canControl`.
If `canControl` is `false`, the server will not send any control commands to the plugin.

### Plugin.Stream.Player.Control

Used to control the player. The property `canControl` must be `true`.

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.Control", "params": {"command": "<command>", "params": { "<param 1>": <value 1>, "<param 2>": <value 2>}}}
```

#### Supported `command`s

* `play`: Start playback (if `canPlay` is `true`)
  * `params`: none
* `pause`: Stop playback (if `canPause` is `true`)
  * `params`: none
* `playPause`: Toggle play/pause (if `canPause` is `true`)
  * `params`: none
* `stop`: Stop playback (if `canControl` is `true`)
  * `params`: none
* `next`: Skip to next track (if `canGoNext` is `true`)
  * `params`: none
* `previous`: Skip to previous track (if `canGoPrevious` is `true`)
  * `params`: none
* `seek`: Seek forward or backward in the current track (if `canSeek` is `true`)
  * `params`:
    * `offset`: [float] seek offset in seconds
* `setPosition`: Set the current track position in seconds (if `canSeek` is `true`)
  * `params`:
    * `position`: [float] the new track position

#### Example

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.Control", "params": {"command": "setPosition", "params": { "position": 17.827 }}}
```

#### Expected response

Success:

```json
{"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

Error:

Any [json-rpc 2.0 conformant error](https://www.jsonrpc.org/specification#error_object) with an error message that helps the user to diagnose the error

### Plugin.Stream.Player.SetProperty

Snapserver will send the `SetProperty` command to the plugin, if `canControl` is `true`.

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.SetProperty", "params": {"<property>": <value>}}
```

#### Supported `property`s

* `loopStatus`: [string] the current repeat status, one of:
  * `none`: the playback will stop when there are no more tracks to play
  * `track`: the current track will start again from the begining once it has finished playing
  * `playlist`: the playback loops through a list of tracks
* `shuffle`: [bool] play playlist in random order
* `volume`: [int] voume in percent, valid range [0..100]
* `mute`: [bool] the current mute state
* `rate`: [float] the current playback rate, valid range (0..)

#### Expected response

##### Success

```json
{"id": 1, "jsonrpc": "2.0", "result": "ok"}
```

##### Error

Any [json-rpc 2.0 conformant error](https://www.jsonrpc.org/specification#error_object) with an error message that helps the user to diagnose the error

### Plugin.Stream.Player.GetProperties

```json
{"id": 1, "jsonrpc": "2.0", "method": "Plugin.Stream.Player.GetProperties"}
```

#### Supported `property`s

* `playbackStatus`: [string] The current playback status, one of:
  * `playing`
  * `paused`
  * `stopped`
* `loopStatus`: [string] The current repeat status, one of:
  * `none`: The playback will stop when there are no more tracks to play
  * `track`: The current track will start again from the begining once it has finished playing
  * `playlist`: The playback loops through a list of tracks
* `shuffle`: [bool] Traverse through the playlist in random order
* `volume`: [int] Voume in percent, valid range [0..100]
* `mute`: [bool] Current mute state
* `rate`: [float] The current playback rate, valid range (0..)
* `position`: [float] Current playback position in seconds
* `canGoNext`: [bool] Whether the client can call the `next` method on this interface and expect the current track to change
* `canGoPrevious`: [bool] Whether the client can call the `previous` method on this interface and expect the current track to change
* `canPlay`: [bool] Whether playback can be started using `play` or `playPause`
* `canPause`: [bool] Whether playback can be paused using `pause` or `playPause`
* `canSeek`: [bool] Whether the client can control the playback position using `seek` and `setPosition`
* `canControl`: [bool] Whether the media player may be controlled over this interface
* `metadata`: [json] message with the following (optional) fields:
  * `trackId`: [string] A unique identity for this track within the context of an MPRIS object (eg: tracklist).
  * `file`: [string] The current song.
  * `duration`: [float] The duration of the song in seconds; may contain a fractional part.
  * `artist`: [list of strings] The track artist(s).
  * `artistSort`: [list of strings] Same as artist, but for sorting. This usually omits prefixes such as “The”.
  * `album`: [string] The album name.
  * `albumSort`: [string] Same as album, but for sorting.
  * `albumArtist`: [list of strings] The album artist(s).
  * `albumArtistSort`: [list of strings] Same as albumartist, but for sorting.
  * `name`: [string] A name for this song. This is not the song title. The exact meaning of this tag is not well-defined. It is often used by badly configured internet radio stations with broken tags to squeeze both the artist name and the song title in one tag.
  * `date`: [string] The song’s release date. This is usually a 4-digit year.
  * `originalDate`: [string] The song’s original release date.
  * `composer`: [list of strings] The composer(s) of the track.
  * `performer`: [string] The artist who performed the song.
  * `conductor`: [string] The conductor who conducted the song.
  * `work`: [string] “a work is a distinct intellectual or artistic creation, which can be expressed in the form of one or more audio recordings”
  * `grouping`: [string] “used if the sound belongs to a larger category of sounds/music” (from the IDv2.4.0 TIT1 description).
  * `comment`: [list of string] A (list of) freeform comment(s)
  * `label`: [string] The name of the label or publisher.
  * `musicbrainzArtistId`: [string] The artist id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `musicbrainzAlbumId`: [string] The album id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `musicbrainzAlbumArtistId`: [string] The album artist id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `musicbrainzTrackId`: [string] The track id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `musicbrainzReleaseTrackId`: [string] The release track id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `musicbrainzWorkId`: [string] The work id in the [MusicBrainz](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html) database.
  * `lyrics`: [list of strings] The lyricist(s) of the track
  * `bpm`: [integer] The speed of the music, in beats per minute.
  * `autoRating`: [float] An automatically-generated rating, based on things such as how often it has been played. This should be in the range 0.0 to 1.0.
  * `comment`: [list of strings] A (list of) freeform comment(s).
  * `composer`: [list of strings] The composer(s) of the track.
  * `contentCreated`: [string] Date/Time: When the track was created. Usually only the year component will be useful.
  * `discNumber`: [integer] The disc number on the album that this track is from.
  * `firstUsed`: [string] Date/Time: When the track was first played.
  * `genre`: [list of strings] List of Strings: The genre(s) of the track.
  * `lastUsed`: [string] Date/Time: When the track was last played.
  * `lyricist`: [list of strings] List of Strings: The lyricist(s) of the track.
  * `title`: [string] The track title.
  * `trackNumber`: [string] The track number on the album disc.
  * `url`: [string uri] The location of the media file.
  * `artUrl`: [string uri] The location of an image representing the track or album. Clients should not assume this will continue to exist when the media player stops giving out the URL.
  * `artData`: [json] Base64 encoded image representing the track or album. if `artUrl` is not specified, Snapserver will decode and cache the image, and will publish the image via `artUrl`.
    * `data`: [string] Base64 encoded image
    * `extension`: [string] The image file extension (e.g. "png", "jpg", "svg")
  * `useCount`: [integer] The number of times the track has been played.
  * `userRating`: [float] A user-specified rating. This should be in the range 0.0 to 1.0.
  * `spotifyArtistId`: [string] The [Spotify Artist ID](https://developer.spotify.com/documentation/web-api/#spotify-uris-and-ids)
  * `spotifyTrackId`: [string] The [Spotify Track ID](https://developer.spotify.com/documentation/web-api/#spotify-uris-and-ids)

#### Expected response

##### Success

```json
{"id": 1, "jsonrpc": "2.0", "result": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":false,"loopStatus":"none","playbackStatus":"playing","position":93.394,"shuffle":false,"volume":86,"mute":false}}

{"id": 1, "jsonrpc": "2.0", "result": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":true,"loopStatus":"none","metadata":{"album":"Doldinger","albumArtist":["Klaus Doldinger's Passport"],"artUrl":"http://coverartarchive.org/release/0d4ff56b-2a2b-43b5-bf99-063cac1599e5/16940576164-250.jpg","artist":["Klaus Doldinger's Passport feat. Nils Landgren"],"contentCreated":"2016","duration":305.2929992675781,"genre":["Jazz"],"musicbrainzAlbumId":"0d4ff56b-2a2b-43b5-bf99-063cac1599e5","title":"Soul Town","trackId":"7","trackNumber":6,"url":"Klaus Doldinger's Passport - Doldinger (2016)/06 - Soul Town.mp3"},"playbackStatus":"playing","position":72.79499816894531,"shuffle":false,"volume":97,"mute":false}}
```

##### Error

Any [json-rpc 2.0 conformant error](https://www.jsonrpc.org/specification#error_object) with an error message that helps the user to diagnose the error

## Notifications

### Plugin.Stream.Player.Properties

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties", "params": {"canControl":true,"canGoNext":true,"canGoPrevious":true,"canPause":true,"canPlay":true,"canSeek":false,"loopStatus":"none","playbackStatus":"playing","position":593.394,"shuffle":false,"volume":86,"mute":false}}
```

Same format as in [GetProperties](#pluginstreamplayergetproperties). If `metadata` is missing, the last known metadata will be used, so the plugin must not send the complete metadata if one of the properties is updated.

### Plugin.Stream.Log

Send a log message

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Log", "params": {"severity":"Info","message":"<Log message>"}}
```

#### Supported `severity`s

* `trace` verbose debug-level message
* `debug` debug-level message
* `info` informational message
* `notice` normal, but significant, condition
* `warning` warning conditions
* `error` error conditions
* `fatal` critical conditions

### Plugin.Stream.Ready

The plugin shall send this notification when it's up and ready to receive commands

```json
{"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"}
```

# Server

TODO: this belongs to doc/json_rpc_api/control.md

## Requests

To control the stream state, the following commands can be sent to the Snapcast server and will be forwarded to the respective stream:

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.Control", "params": {"id": "Pipe", "command": command, "params": params}}
```

```json
{"id": 1, "jsonrpc": "2.0", "method": "Stream.SetProperty", "params": {"id": "Pipe", "property": property, "value": value}}
```

## Notifications

```json
{"jsonrpc": "2.0", "method": "Stream.OnProperties", "params": {"id": "Pipe", "properties": {}}}
```
