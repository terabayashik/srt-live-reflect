# srt-live-reflect
reflect srt live stream

## build
### requirements
* libsrt
* libcurl
* boost

## args
### conf=*{{path to conf file}}*
  * **default** : *{{excecutable path}}* + *".conf"*

## conf
### srt-live-stream.conf
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
