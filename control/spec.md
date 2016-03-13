Snapcast control protocol
=========================

#Stream
###Stream status
```json
{
  "id": "pipe:///tmp/snapfifo",
  "status": "playing",
  "uri": {
    "fragment": "",
    "host": "",
    "path": "/tmp/snapfifo",
    "query": {
      "buffer_ms": "20",
      "codec": "flac",
      "name": "Radio",
      "sampleformat": "48000:16:2"
    },
    "raw": "pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac",
    "scheme": "pipe"
  }
}
```

###Stream update push notification
```json
{
  "jsonrpc": "2.0",
  "method": "Stream.OnUpdate",
  "params": {
    "data": {
      "id": "pipe:///tmp/snapfifo",
      "status": "playing",
      "uri": {
        "fragment": "",
        "host": "",
        "path": "/tmp/snapfifo",
        "query": {
          "buffer_ms": "20",
          "codec": "flac",
          "name": "Radio",
          "sampleformat": "48000:16:2"
        },
        "raw": "pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac",
        "scheme": "pipe"
      }
    }
  }
}
```

#Client
##Client status
```json
{
  "config": {
    "latency": 0,
    "name": "",
    "stream": "pipe:///tmp/snapfifo",
    "volume": {
      "muted": false,
      "percent": 75
    }
  },
  "connected": true,
  "host": {
    "arch": "unknown",
    "ip": "192.168.0.24",
    "mac": "80:1f:02:ed:fd:e0",
    "name": "wohnzimmer",
    "os": "Raspbian GNU/Linux 8.0 (jessie)"
  },
  "lastSeen": {
    "sec": 1457597583,
    "usec": 956325
  },
  "snapclient": {
    "name": "Snapclient",
    "protocolVersion": 1,
    "version": "0.5.0-beta-2"
  }
}
```

#Server 
##Server status
```json
{
  "id": 0,
  "jsonrpc": "2.0",
  "result": {
    "clients": [
      {
        "config": {
          "latency": 0,
          "name": "",
          "stream": "pipe:///tmp/snapfifo",
          "volume": {
            "muted": false,
            "percent": 75
          }
        },
        "connected": true,
        "host": {
          "arch": "unknown",
          "ip": "192.168.0.24",
          "mac": "80:1f:02:ed:fd:e0",
          "name": "wohnzimmer",
          "os": "Raspbian GNU/Linux 8.0 (jessie)"
        },
        "lastSeen": {
          "sec": 1457597583,
          "usec": 956325
        },
        "snapclient": {
          "name": "Snapclient",
          "protocolVersion": 1,
          "version": "0.5.0-beta-2"
        }
      },
      {
        "config": {
          "latency": 0,
          "name": "Galaxy S5",
          "stream": "pipe:///tmp/snapfifo",
          "volume": {
            "muted": false,
            "percent": 35
          }
        },
        "connected": false,
        "host": {
          "arch": "armeabi-v7a",
          "ip": "192.168.0.23",
          "mac": "a0:b4:a5:3a:f1:db",
          "name": "android-6bd0a5e5c0068caf",
          "os": "Android 5.0.2"
        },
        "lastSeen": {
          "sec": 1457594760,
          "usec": 278026
        },
        "snapclient": {
          "name": "Snapclient",
          "protocolVersion": 1,
          "version": "0.5.0-beta-2"
        }
      },
      {
        "config": {
          "latency": 0,
          "name": "",
          "stream": "pipe:///tmp/snapfifo",
          "volume": {
            "muted": false,
            "percent": 76
          }
        },
        "connected": false,
        "host": {
          "arch": "x86_64",
          "ip": "192.168.0.54",
          "mac": "00:21:6a:7d:74:fc",
          "name": "T400",
          "os": "Linux Mint 17.3 Rosa"
        },
        "lastSeen": {
          "sec": 1457594824,
          "usec": 708630
        },
        "snapclient": {
          "name": "Snapclient",
          "protocolVersion": 1,
          "version": "0.5.0-beta-2"
        }
      }
    ],
    "server": {
      "host": {
        "arch": "x86_64",
        "ip": "",
        "mac": "",
        "name": "elaine",
        "os": "Linux Mint 17.3 Rosa"
      },
      "snapserver": {
        "controlProtocolVersion": 1,
        "name": "Snapserver",
        "protocolVersion": 1,
        "version": "0.5.0-beta-2"
      }
    },
    "streams": [
      {
        "id": "pipe:///tmp/snapfifo",
        "status": "idle",
        "uri": {
          "fragment": "",
          "host": "",
          "path": "/tmp/snapfifo",
          "query": {
            "buffer_ms": "20",
            "codec": "flac",
            "name": "Radio",
            "sampleformat": "48000:16:2"
          },
          "raw": "pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac",
          "scheme": "pipe"
        }
      },
      {
        "id": "file:///home/johannes/Intern/Music/Wave file.wav",
        "status": "playing",
        "uri": {
          "fragment": "",
          "host": "",
          "path": "/home/johannes/Intern/Music/Wave file.wav",
          "query": {
            "buffer_ms": "20",
            "codec": "ogg:VBR:0.1",
            "name": "AL",
            "sampleformat": "48000:16:2"
          },
          "raw": "file:///home/johannes/Intern/Music/Wave%20file.wav?name=AL&sampleformat=48000:16:2&codec=ogg:VBR:0.1",
          "scheme": "file"
        }
      }
    ]
  }
}
```

