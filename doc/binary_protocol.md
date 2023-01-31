# Snapcast binary protocol

Each message sent with the Snapcast binary protocol is split up into two parts:
- A base message that provides general information like time sent/received, type of the message, message size, etc
- A typed message that carries the rest of the information

The protocol is using little endian.

## Client joining process

When a client joins a server, the following exchanges happen

1. Client opens a TCP socket to the server (default port is 1704)
1. Client sends a [Hello](#hello) message
1. Server sends a [Server Settings](#server-settings) message
1. Server sends a [Codec Header](#codec-header) message
    1. Until the server sends this, the client shouldn't play any [Wire Chunk](#wire-chunk) messages
1. The server will now send [Wire Chunk](#wire-chunk) messages, which can be fed to the audio decoder.
1. When it comes time for the client to disconnect, the socket can just be closed.
1. Client periodically sends a [Time](#time) message, carrying a sent timestamp `t_client-sent`
    1. Receives a Time response containing the client to server time delta `latency_c2s = t_server-recv - t_client-sent + t_network-latency` and the server sent timestamp `t_server-sent`
    1. Calculates `latency_s2c = t_client-recv - t_server-sent + t_network_latency`
    1. Calcutates the time diff between server and client as `(latency_c2s - latency_s2c) / 2`, eliminating the network latency (assumed to be symmetric)

## Messages

| Typed Message ID | Name                                 | Notes                                                                     |
|------------------|--------------------------------------|---------------------------------------------------------------------------|
| 0                | [Base](#base)                        | The beginning of every message containing data about the typed message    |
| 1                | [Codec Header](#codec-header)        | The codec-specific data to put at the start of a stream to allow decoding |
| 2                | [Wire Chunk](#wire-chunk)            | A part of an audio stream                                                 |
| 3                | [Server Settings](#server-settings)  | Settings set from the server like volume, latency, etc                    |
| 4                | [Time](#time)                        | Used for synchronizing time with the server                               |
| 5                | [Hello](#hello)                      | Sent by the client when connecting with the server                        |
| 6                | [Stream Tags](#stream-tags)          | Metadata about the stream for use by the client                           |

### Base

| Field                 | Type   | Description                                                                                       |
|-----------------------|--------|---------------------------------------------------------------------------------------------------|
| type                  | uint16 | Should be one of the typed message IDs                                                            |
| id                    | uint16 | Used in requests to identify the message (not always used)                                        |
| refersTo              | uint16 | Used in responses to identify which request message ID this is responding to                      |
| sent.sec              | int32  | The second value of the timestamp when this message was sent. Filled in by the sender.            |
| sent.usec             | int32  | The microsecond value of the timestamp when this message was sent. Filled in by the sender.       |
| received.sec          | int32  | The second value of the timestamp when this message was received. Filled in by the receiver.      |
| received.usec         | int32  | The microsecond value of the timestamp when this message was received. Filled in by the receiver. |
| size                  | uint32 | Total number of bytes of the following typed message                                              |

### Codec Header

| Field      | Type    | Description                                                 |
|------------|---------|-------------------------------------------------------------|
| codec_size | unint32 | Length of the codec string (not including a null character) |
| codec      | char[]  | String describing the codec (not null terminated)           |
| size       | uint32  | Size of the following payload                               |
| payload    | char[]  | Buffer of data containing the codec header                  |

The payload depends on the used codec:

- Flac: the FLAC audio file header, as described [here](https://www.the-roberts-family.net/metadata/flac.html#:~:text=Overall%20Structure&text=It%20has%20four%20parts%3A%20a,and%20the%20actual%20audio%20data.). The decoder must be initialized with this header.
- Ogg: the vorbis stream header, as described [here](https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-610004.2). The decoder must be initialized with this header.
- PCM: a RIFF WAVE header, as described [here](https://de.wikipedia.org/wiki/RIFF_WAVE). PCM is not encoded, but the decoder must know the samplerate, bit depth and number of channels, which is encoded into the header
- Opus: a dummy header is sent, containing a 4 byte ID (0x4F505553, ascii for "OPUS"), 4 byte samplerate, 2 byte bit depth, 2 byte channel count (all little endian)


### Wire Chunk

| Field          | Type    | Description                                                                           |
|----------------|---------|---------------------------------------------------------------------------------------|
| timestamp.sec  | int32   | The second value of the timestamp when this part of the stream was recorded           |
| timestamp.usec | int32   | The microsecond value of the timestamp when this part of the stream was recorded      |
| size           | uint32  | Size of the following payload                                                         |
| payload        | char[]  | Buffer of data containing the encoded PCM data (a decodable chunk per message)        |

### Server Settings

| Field   | Type   | Description                                              |
|---------|--------|----------------------------------------------------------|
| size    | uint32 | Size of the following JSON string                        |
| payload | char[] | JSON string containing the message (not null terminated) |

Sample JSON payload (whitespace added for readability):

```json
{
    "bufferMs": 1000,
    "latency": 0,
    "muted": false,
    "volume": 100
}
```

- `volume` can have a value between 0-100 inclusive

### Time

| Field          | Type    | Description                                                            |
|----------------|---------|------------------------------------------------------------------------|
| latency.sec    | int32   | The second value of the latency between the server and the client      |
| latency.usec   | int32   | The microsecond value of the latency between the server and the client |

### Hello

| Field   | Type   | Description                                              |
|---------|--------|----------------------------------------------------------|
| size    | uint32 | Size of the following JSON string                        |
| payload | char[] | JSON string containing the message (not null terminated) |

Sample JSON payload (whitespace added for readability):

```json
{
    "Arch": "x86_64",
    "ClientName": "Snapclient",
    "HostName": "my_hostname",
    "ID": "00:11:22:33:44:55",
    "Instance": 1,
    "MAC": "00:11:22:33:44:55",
    "OS": "Arch Linux",
    "SnapStreamProtocolVersion": 2,
    "Version": "0.17.1"
}
```
