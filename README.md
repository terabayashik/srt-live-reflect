# srt-live-reflect
[![CodeQL](https://github.com/wakabayashik/srt-live-reflect/actions/workflows/codeql.yml/badge.svg)](https://github.com/wakabayashik/srt-live-reflect/actions/workflows/codeql.yml)

reflect srt live stream

## § build
### requirements
* libsrt
  * vcpkg install libsrt:x86-windows-static
  * vcpkg install libsrt:x64-windows-static
* libcurl
  * vcpkg install curl:x86-windows-static
  * vcpkg install curl:x64-windows-static
* aws-sdk-cpp (optional: *USE_AWSSDK=1*)
  * vcpkg install aws-sdk-cpp:x64-windows-static
  * vcpkg install aws-sdk-cpp:x86-windows-static
* boost

## § command line arguments
### conf=**{{path to configuration file}}**
* **default** : *"./srt-live-reflect.conf"*

## § configuration file
### srt-live-reflect.conf (JSON)
* acccepts C style, C++ style comment and trailing commas
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
  "aws": {
    "enabled": false,          // enable or disable AWSSDK (default:false)
    "loglevel": "error",       // AWSSDK log level ["trace" / "debug" / "info" / "warning" / "error" / "fatal"] (default:fallback to "logger.level")
    "logprefix": "AWSSDK",     // prefix for logs from AWSSDK (default:"AWSSDK")
    "region": "ap-northeast-1",// AWS region to be used (default:not specified)
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
    },
    "loopRecs": [{
      "name": "stream-A",        // resource name to be recorded
      "dir": "./stream-A",       // path to directory where the recorded files will be created (default:"./" + resource name)
      "data_extension": ".dat",  // extension for data files (default:".dat")
      "index_extension": ".idx", // extension for index files (default:".idx")
      "segment_duration": 600,   // duration of the recorded file per segment in seconds (default:600)
      "total_duration": 3600,    // total duration of loop recording in seconds (default:3600)
      "index_interval": 100,     // indexing interval for a recording file in milliseconds (default:100)
      "prefetch": 1000,          // time (in milliseconds) when to start prefetching the next segment during playback (0 to disable prefetch) (default:1000)
      "queue": 0,                // maximum time (in milliseconds) to queue the ingress data when recording (0 to disable queue) (default:0)
      "s3": {                    // "aws.enabled" should be set to true when using AWS S3
        "bucket": "bucket-A",    // AWS S3 bucket name to store the recorded files (empty to disable S3 upload) (default:"")
        "folder": "stream-A",    // folder name on AWS S3 bucket (default:hostname + "/" + resource name)
        "bufsiz": 18800,         // buffer size used when playback the stream from AWS S3 (default:188*100)
      },
    }]
  }]
}
```

## § option
* (refs.) [SRT API Socket Options](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md)

## § streamid
* (refs.) [SRT Access Control (Stream ID) Guidelines](https://github.com/Haivision/srt/blob/master/docs/features/access-control.md)
* in case of ffmpeg, streamid is specified by **-srt_streamid** option

### common
* **r** : resource name identifies the name of the resource
* **m** : mode expected for this connection
  * **request** (default) : the caller wants to receive the stream
  * **publish** : the caller wants to send the stream data

### for playback of loop recording
* **at** : specifies requested position of recorded data
  * **ISO string** : specifies the date and time to be requested in ISO8601 syntax (ex.:"20230317T123000+0900")
  * **now-{{sec}}** : specifies the time seconds before the current time
  * **now** (default) : specifies live stream instead of recorded data
* **gap** : specifies the action to be taken when the stream reaches a gap in the recorded data.
  * **skip** (default) : skip to the next recorded data without waiting
  * **wait** : wait for a time corresponding to the gap before starting the next recorded data
  * **break** : break the stream when a gap appears
* **speed** : specifies playback speed (synonym: **x**)
  * **1** (default) : normal play speed

## § service (windows)
* make it service with [nssm](https://nssm.cc/)

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

## § service (linux)
* make it service with systemd

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

## § work with ffmpeg

### publish
> ffmpeg -re -stream_loop -1 -i **{{path to movie file}}** -c:v copy -c:a copy -f mpegts -pes_payload_size 0 -srt_streamid #!::r=**{{stream name}}**,m=publish srt://**{{host}}**:**{{port}}**

### playback
> ffplay -srt_streamid #!::**{{stream name}}**,m=request srt://**{{host}}**:**{{port}}**
