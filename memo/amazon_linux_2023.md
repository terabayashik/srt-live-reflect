## (memo) amazon linux 2023

## § build

### requirements

```sh
]$ cd
]$ pwd
/home/ec2-user
]$ mkdir -p tmp && cd tmp
]$ sudo dnf install g++ -y
]$ sudo dnf install perl -y
]$ sudo dnf install perl-IPC-Cmd -y
]$ sudo dnf install git -y
]$ git clone https://github.com/microsoft/vcpkg
]$ ./vcpkg/bootstrap-vcpkg.sh
]$ sudo ./vcpkg/vcpkg install openssl:x64-linux
]$ sudo ./vcpkg/vcpkg install libsrt:x64-linux
]$ sudo ./vcpkg/vcpkg install curl:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-algorithm:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-format:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-tokenizer:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-lexical-cast:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-thread:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-range:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-json:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-tuple:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-date-time:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-xpressive:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-filesystem:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-endian:x64-linux
]$ sudo ./vcpkg/vcpkg install boost-log:x64-linux
]$ sudo ./vcpkg/vcpkg install aws-sdk-cpp[s3]:x64-linux
```

### make

```sh
]$ git clone https://github.com/wakabayashik/srt-live-reflect
]$ cd srt-live-reflect
]$ git checkout develop
]$ make USE_AWSSDK=1
```

## § S3 access permission

### IAM Role

* prepare IAM role with following policy.
* attach the role to EC2 instance to permit access to S3 bucket.

```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "s3:ListBucket",
                "s3:GetObject",
                "s3:PutObject",
                "s3:DeleteObject"
            ],
            "Resource": [
                "arn:aws:s3:::{{bucket-name}}",
                "arn:aws:s3:::{{bucket-name}}/*"
            ]
        }
    ]
}
```
