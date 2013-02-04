proxyrouter
===========
As its name indicates, proxyrouter is a http layer router. Its concept quite resembles the chrome plugins SwitchySharp. 

Suppose we have 2 HTTP proxies, namely squid on myserver:3128 and polipo on myserver:8123. We want sites that matched list1 be visited via squid and sites matched list 2 visited via polipo. If no list is matched, then access it directly.

It does not support authentication. If you really need it, configure a squid as its child and use squid's authentication mechnism.