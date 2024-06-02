# realtime-text

## Introduction

A simple collaborative editing server. It is still in pre-alpha stage.

## Compile and Run

Currently only Linux is tested and supported. To compile:
```
g++ sync_server.cpp client.cpp file.cpp -O2 -o sync_server
```

Then, start it with the base directory as argument:
```
./sync_server path_to_basedir
```

The client is a patched nano editor, check my other repo `nano-client` for usage.
