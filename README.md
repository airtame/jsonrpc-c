jsonrpc-c
=========

JSON-RPC in C (server and a basic version of client)

What?
-----
A library for a C program to receive or make JSON-RPC requests on tcp sockets (no HTTP).

Free software, MIT license.

Why?
----
I needed a way for an application written in C, running on an embedded Linux system to be configured by
a Java/Swing configuration tool running on a connected laptop. Wanted something simple, human readable,
and saw no need for HTTP.

How?
----
It depends on libev (was already used on the embedded app) and includes cJSON (with a small patch on my fork).
No further dependencies.

###Testing

Run `autoreconf -i`  before `./configure` and `make`

Test the example server by running it and typing: 

`echo "{\"method\":\"sayHello\"}" | nc localhost 1234`

or

`echo "{\"method\":\"exit\"}" | nc localhost 1234`

or
`example/client 127.0.0.1 500  1 4`
which will add 1 to 4 on the server 500 times.

Who?
----

@hmngomes