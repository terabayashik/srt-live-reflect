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
* **default** : *{{path to srt-live-reflect executable}}* + *".conf"*
### cainfo=*{{path to certificate authority (CA) bundle}}*
* certificate authority (CA) to verify peer in https access with libcurl
* **default** : skip verification

## conf
### srt-live-stream.conf (JSON)
* allow c style, c++ style comment and trailing commas
```json
{
  "name": "srt-live-reflect",
  "reflects": [{
    "app": "live",
    "port": 14501,
    "backlog": "5",
    "option": { // srt options
      "udpsndbuf": 65536,
      "udprcvbuf": 65536
    },
    "publish": {
      "stats": 60, // period to print statistics in seconds (0: disabled)
      "option": { // srt options for publish
        "linger": 0
      },
      "access": [ // static access control for publish
        {"allow":"192.168.11.0/24", "name":"stream-*"}, // allow publish with streamid "#!::r=stream-xxx,m=publish" from 192.168.11.0/24
        {"deny":"all"} // deny all others
      ],
      "on_pre_accept": "http://127.0.0.1:8090/on_pre_accept_publish",
      // "on_accept": "http://127.0.0.1:8090/on_accept_publish"
    },
    "play": {
      "option": { // srt options for play
        "maxbw": 0,
        "inputbw": 0,
        "oheadbw": 25
      },
      "access": [ // static access control for play
        {"allow":"127.0.0.1"},
        {"deny":"all"}
      ],
      "on_pre_accept": "http://127.0.0.1:8090/on_pre_accept_play",
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
set retentionDays=100

mkdir "%dir%\logs"
"%nssm%" install srt-live-reflect "%dir%\srt-live-reflect.exe"
"%nssm%" set srt-live-reflect DisplayName srt-live-reflect
"%nssm%" set srt-live-reflect Description srt-live-reflect service
"%nssm%" set srt-live-reflect AppDirectory "%dir%"
"%nssm%" set srt-live-reflect AppStdout "%dir%\logs\srt-live-reflect.log"
"%nssm%" set srt-live-reflect AppStderr "%dir%\logs\srt-live-reflect.log"
"%nssm%" set srt-live-reflect AppRotateFiles 1
"%nssm%" set srt-live-reflect AppRotateOnline 1
"%nssm%" set srt-live-reflect AppRotateBytes 10485760

schtasks /Create /TN "srt-live-reflect_log-delete" /RU SYSTEM /SC DAILY /ST 00:05:00 /F /TR "forfiles /P '%dir%\logs' /D -%retentionDays% /M srt-live-reflect-*.log /C 'cmd /c if @isdir==FALSE del /s @path'"
schtasks /Create /TN "srt-live-reflect_log-rotate" /RU SYSTEM /SC DAILY /ST 00:00:00 /F /TR "%nssm% rotate srt-live-reflect"

net start srt-live-reflect
```

### remove
```bat
set nssm={{path to nssm.exe}}

net stop srt-live-reflect

"%nssm%" remove srt-live-reflect confirm

schtasks.exe /Delete /TN "srt-live-reflect_log-delete" /F
schtasks.exe /Delete /TN "srt-live-reflect_log-rotate" /F
```
