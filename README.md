Proxior
===========
Proxior can be seen as an HTTP layer router. Its concept quite resembles the chrome plugins SwitchySharp and PAC. 

It supports HTTP proxy and direct link, and will support socks5 proxy sometime.

Proxior does not support authentication at this time. If you really need it, configure a squid proxy and set Proxior to be its parent proxy.

Compilation
===========
libevent is required.

make && sudo make install

Tested with Ubuntu and Mac OSX.

Configuration
===========
The config and access lists are located at /etc/proxior/. Go there to see details.