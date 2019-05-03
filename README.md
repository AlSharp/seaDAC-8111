# seaDAC-8111 Node.js FFI Demo

## Requirements
1. Linux
2. python 2.7 and gcc compliler
3. node-gyp (npm install -g node-gyp)

## Allow user read and write usb devices
Create file /etc/udev/rules.d/50-usb.rules and add following line
KERNEL=="ttyUSB*", GROUP="username", MODE="0666"

## How to run

1. Clone repo
2. npm install
3. npm start



