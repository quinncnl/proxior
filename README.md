Proxior
===========
Proxior can be seen as an HTTP layer router or a proxy manager. Its concept quite resembles the chrome plugins SwitchySharp and PAC. 

Proxior可被看作一个HTTP层的路由软件或是一个代理管理工具。它的工作方式很像Chrome插件SwitchySharp或者PAC文件。

It supports HTTP proxy and direct link, and will support socks5 proxy sometime.

目前支持HTTP代理和直接连接，将来会支持socks5代理。

Proxior does not support authentication at this time. If you really need it, configure a Squid Cache and set Proxior to be its parent proxy.

目前不支持HTTP代理认证，如果你需要的话，配置一个Squid并令其父代理为Proxior.

Installation
===========
libevent 2.0.x is required。

libevent 2.0.x是必要的。

On Ubuntu,

在Ubuntu下,

1) sudo apt-get install libevent-dev

2) make

After compilation succeeded, you can run it with:

编译成功后可以直接使用如下命令使用:

./proxior -p ./

Install into system: sudo make install

Uninstall: sudo make uninstall

Tested with Ubuntu and Mac OSX.

在Ubuntu 12.04和Mac下测试通过。

Configuration
===========
The config and access lists are located at /etc/proxior/. Go there to see details.

安装后配置和日志在/etc/proxior/目录。

There is a remote control at: http://strongwillow.github.com/proxior/control/. It can be run on any web server and even local file system.

线上控制端: http://strongwillow.github.com/proxior/control/. 你可以在这里增删列表内容，它是静态HTML文件。
