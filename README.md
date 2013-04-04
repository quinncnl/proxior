## Proxior

Proxior can be seen as an HTTP layer router or a proxy manager. Its concept quite resembles the chrome plugins SwitchySharp and PAC. 

It supports HTTP proxy and socks5 proxy.

Proxior does not support authentication at this time. If you really need it, configure a Squid Cache and set Proxior to be its parent proxy.

## Installation

libevent 2.0.x is required。

On Ubuntu,

1) sudo apt-get install libevent-dev

2) make

After compilation succeeded, you can run it with:

./proxior -p ./

Install into system: sudo make install

Uninstall: sudo make uninstall

Tested with Ubuntu and Mac OSX.

## Configuration

The config and access lists are located at /etc/proxior/. Go there to see details.

There is a remote control at: http://strongwillow.github.com/proxior/control/. It can be run on any web server and even local file system.
