# seaDAC-8111 Node.js FFI Demo

## Requirements
1. Linux
2. python 2.7 and gcc compliler
3. node-gyp (npm install -g node-gyp)

## Allow user read and write usb deice
Add following rule into file /etc/udev/rules.d/50-udev-default.rules
SUBSYSTEM=="usb",ATTRS{idVendor}=="0c52",ATTRS{idProduct}=="8111",MODE="0666",GROUP="pi",SYMLINK+="seadac%n"

## How to run

1. Clone repo
2. npm install
3. npm start



