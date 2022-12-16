# srt-live-reflect
reflect srt live stream

## build
### requirements
* libsrt
  * vcpkg install libsrt:x86-windows-static
  * vcpkg install libsrt:x64-windows-static
* libcurl
  * vcpkg install curl:x86-windows-static
  * vcpkg install curl:x64-windows-static
* boost

## args
### conf=*{{path to conf file}}*
* **default** : *"./srt-live-reflect.conf"*

## conf
### srt-live-stream.conf (JSON)
* allow c style, c++ style comment and trailing commas
```json
{
  "name": "srt-live-reflect",
  "cainfo": "",                // path to certificate authority (CA) bundle (empty to skip CA verification) (default:"")
  "srtloglevel": "error",      // srt log level ["debug" / "note" / "warning" / "error" / "fatal"] (default:"error")
  "logger": {
    "target": "",              // path to log directory (empty to disable logging) (default:"")
    "level": "info",           // log level ["trace" / "debug" / "info" / "warning" / "error" / "fatal"] (default:"info")
    "max_size": 1073741824,    // maximum total log size (default:1024*1024*1024)
    "max_files": 30,           // maximum number of log files (default:30)
  },
  "reflects": [{
    "app": "live",
    "port": 14501,
    "backlog": "5",
    "option": {                // srt options (pre-bind)
      "udpsndbuf": 65536,
      "udprcvbuf": 65536,
      // ... (see:option)
    },
    "publish": {
      "stats": 600,            // period to print statistics in seconds (0:disabled) (default:0)
      "option": {              // srt options for publish (pre)
        "linger": 0,
        // ... (see:option)
      },
      "access": [              // static access control for publish
        {
          "allow":"192.168.11.0/24", // allow from 192.168.11.0/24
          "name":"stream-*"          // apply only resouce name matches (default:"*") (see:streamid)
        },
        {"deny":"all"}         // deny all others
      ],
      "on_pre_accept": "http://127.0.0.1:8090/on_pre_accept_publish", // dynamic access control for publish
      // "on_accept": "http://127.0.0.1:8090/on_accept_publish"       // comment out
    },
    "play": {
      "option": {              // srt options for play (pre)
        "maxbw": 0,
        "inputbw": 0,
        "oheadbw": 25,
        // ... (see:option)
      },
      "access": [              // static access control for play
        {"allow":"127.0.0.1"},
        {"deny":"all"}
      ],
      // "on_pre_accept": "http://127.0.0.1:8090/on_pre_accept_play",
      // "on_accept": "http://127.0.0.1:8090/on_accept_play"
    }
  }]
}
```

## option
[SRT API Socket Options](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md)

## streamid
[SRT Access Control (Stream ID) Guidelines](https://github.com/Haivision/srt/blob/master/docs/features/access-control.md)

## service (windows)
make it service with [nssm](https://nssm.cc/)

### install
```bat
set nssm={{path to nssm.exe}}
set dir={{directory where srt-live-reflect is}}
"%nssm%" install srt-live-reflect "%dir%\srt-live-reflect.exe"
"%nssm%" set srt-live-reflect DisplayName srt-live-reflect
"%nssm%" set srt-live-reflect Description srt-live-reflect service
"%nssm%" set srt-live-reflect AppDirectory "%dir%"
net start srt-live-reflect
```

### remove
```bat
set nssm={{path to nssm.exe}}
net stop srt-live-reflect
"%nssm%" remove srt-live-reflect confirm
```

## service (linux)
make it service with systemd

### {{directory where srt-live-reflect is}}/startup.sh
```sh
#! /bin/sh
cd `dirname $0`
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64:/usr/local/lib
./srt-live-reflect conf=./srt-live-reflect.conf
```

### /etc/systemd/system/srt-live-reflect.service
```
[Unit]
Description=srt-live-reflect service
After=network.target

[Service]
Type=simple
ExecStart={{directory where srt-live-reflect is}}/startup.sh
Restart=always
User={{user}}
Group={{group}}

[Install]
WantedBy=multi-user.target
```

## work with ffmpeg

### publish
> ffmpeg -re -stream_loop -1 -i **{{path to movie file}}** -c:v copy -c:a copy -f mpegts -pes_payload_size 0 -srt_streamid #!::r=**{{stream name}}**,m=publish srt://**{{host}}**:**{{port}}**

### playback
> ffplay -srt_streamid #!::**{{stream name}}**,m=request srt://**{{host}}**:**{{port}}**
