# realtime-text

## Introduction

A simple collaborative editing server. It is still in pre-alpha stage.

## Compile and Run

Only Linux is tested and supported for now. To compile:
```
g++ main.cpp client.cpp file.cpp -O2 -o rttcollab
```

Then, start it with the base directory as argument:
```
./rttcollab path_to_basedir
```

Currently, the only client is a patched nano editor, check my other repo `nano-client` for usage.
