# Network objects for Lua scripts

Every Lua script of opendatacon (a port, a transform, a log sink or a UI command script) can talk to the network using ODC's own, tested socket layer, callback based and without blocking: TCP and TLS sockets, an HTTP(S) server and client, and HTTP helpers, in the `odc` table. Whether a script gets them is decided by its host's `NetEnabled` setting. For the rest of the `odc` table (events, timers, logging, JSON) see `ExampleConfig/SelfDocumentingLuaPort/LuaPortDoco.lua`.

## Which scripts get them: NetEnabled

Each Lua host takes a `NetEnabled` setting: a port in its configuration (next to `LuaFile`), a transform in its `Parameters`, the log sink in its `LuaLog` section, and the LuaUICommander plugin in its configuration file or `ConfOverrides`.

| `NetEnabled` | Port, transform, log sink (the script is its `LuaFile`) | UI command script from `ExecuteFile` | UI command script from `Base64Execute` (code typed into a UI) |
|---|---|---|---|
| `true` | network | network | network |
| `false` | none | none | none |
| `"FILE_SCRIPTS_ONLY"` (the default) | network | network | none |
| `{ "Allow": [ "dir/", "file.lua", ... ] }` | network if `LuaFile` is a listed file or under a listed directory | network if the file is | none |

* Paths are compared after resolving symbolic links and `..` (a link inside an allowed directory to a file outside it is not allowed), and relative paths are relative to opendatacon's working directory, as `LuaFile` is.
* Any other value is a configuration error: it is logged, and the script gets no network objects (fail closed).
* The host logs, when it loads a script, whether the script got them and why.
* A script without network access finds `odc.TCPSocket`, `odc.TLSSocket`, `odc.HTTPServer`, `odc.HTTPClient` and `odc.HTTP` to be `nil`.
* It is about the script file: a module the script loads with `require` shares its `odc` table, and so whatever it has.
* With the default, a UI user can run a script file that is already on disk with network access, but cannot give it code of their own. To allow only scripts you have approved, list them, or their directory, in `Allow`. (A log sink added from a UI, `add_logsink ... LUA <file>`, has no configuration: it gets the default.)

## The Lua hosts

What every host has in common: the callbacks run on the script's strand, one at a time, and when the host is destroyed whatever the script left open is torn down without a callback. Where a script closes what it opened depends on the host:

| Host | Where the script closes its objects | Notes |
|---|---|---|
| LuaPort | `Disable()`: disabling the port does not close them for it | |
| LuaTransform | `Disable()`, as a port | |
| LuaLogSink | nowhere before the sink is destroyed (normally at shutdown): it has no other hook | What the sink's own network objects log starts with the sink's name, which the sink is never given, so a `LogSink()` that writes each line to a socket does not feed on itself. A `LogSink()` that logs is still a bad idea. |
| LuaUICommander | `Cancel()`, called when the script is cancelled (`CancelExecution`, `ClearAll`) or the plugin is destroyed while it runs; and before the last return of `SendCommands()` (nil or a negative number) | A script that finishes without closing leaves its objects running, with their callbacks, until it is purged (at the next `ExecuteFile`/`Base64Execute`, `ClearAll` or shutdown), when they are torn down without a callback. Between calls of `SendCommands()` the strand is free for the callbacks. |

## Network objects: overview

| Object | What it is | Built on |
|---|---|---|
| [`odc.TCPSocket`](#odctcpsocket) | a TCP client or (one connection at a time) server | `TCPSessionManager` |
| [`odc.TLSSocket`](#odctlssocket) | the same with TLS 1.2/1.3, client or server, mutual TLS | `TCPSessionManager` with TLS |
| [`odc.HTTPServer`](#odchttpserver) | an HTTP or HTTPS server with routes, serving many connections at once | Simple-Web-Server (SWS) |
| [`odc.HTTPClient`](#odchttpclient) | an HTTP or HTTPS client, any number of requests in flight | SWS |
| [`odc.HTTP`](#odchttp) | pure helpers: percent encoding, query strings, status names, dates | |

Four complete example configurations that use them to carry ODC point events and controls between two ports are described in [Moving ODC events over a network](#moving-odc-events-over-a-network).

**Where they exist.** In every Lua script of opendatacon: ports, transforms, log sinks and UI command scripts, as its `NetEnabled` setting allows (see [Which scripts get them: NetEnabled](#which-scripts-get-them-netenabled)). The sockets run on ODC's own io threads, as do the HTTP objects: no script gets a thread of its own, every callback comes on the script's strand, and nothing blocks the host.

**The rules every object follows.**

* **Options** are one table. The constructor returns the object, or `nil, "<error string>"` (`invalid_options: <name>` names the first option that is wrong; an option it does not know is wrong too, so a typo is not ignored). Nothing is opened until `:open()` (sockets) or `:start()` (server); an HTTP client connects on its first request. Methods use `:`; those that can fail return `true` or `nil, error_string`.
* **Callbacks** run on the script's strand, one at a time, in order, **never from inside the call that triggered them**. An error raised in a callback is caught, logged with the name of the port, transform, log sink or UI script, and does not stop other callbacks. Data is binary-safe Lua strings; times are milliseconds (`..._ms`) except where a name says `_s` or `Secs`; sizes are bytes.
* **Lifetime.** An object keeps itself alive while it is open or has work in flight, so a script need not hold a reference: **collecting the last Lua reference to an open socket, a started server or a client with requests in flight does not close it.** It ends with `:close()` / `:stop()` or a failure it does not recover from. **A script closes what it opens**, where its host gives it the chance (see [The Lua hosts](#the-lua-hosts)): nothing is closed for it, so objects left open keep their sockets, listeners and callbacks running (while a port is disabled, say). When the host is **destroyed** (opendatacon shutting down, a finished UI script purged) whatever is still open is torn down and **no callback is made** (not even `on_close` or a request's `on_response`); the script has no chance to act then, so it is not a substitute for closing. A closed object that is collected is destroyed; the native object is never destroyed from inside one of its own callbacks.
* **Errors are short stable strings** (the table below), derived from error codes and OpenSSL's own results, never from message text; where there is more to say it is a free-text `detail` as the next value, to log and not to compare.

**Error strings: which object can give which.** S = `odc.TCPSocket`, L = `odc.TLSSocket`, H = `odc.HTTPServer`, C = `odc.HTTPClient`.

| String | S | L | H | C | Meaning |
|---|:-:|:-:|:-:|:-:|---|
| `refused` | | | | x | the connection was refused |
| `resolve_failed` | | | | x | the host name could not be resolved |
| `unreachable` | | | | x | no route to the host or network |
| `timeout` | x | x | | x | a connect, idle or request deadline passed (the `detail` says which, where it is known) |
| `closed` | x | x | x | x | the connection ended, or the object is closed / the server stopped (a refused call on a closed object, or `res:write` after `stop()`) |
| `aborted` | x | x | x | x | cancelled by `cancel()` / `close()` (client), or the host was destroyed |
| `bind_failed` | | | x | | a server could not listen |
| `address_in_use` | | | x | | the port is taken |
| `failed` | | | x | x | any other error (the detail has the system's message) |
| `already_written` | | | x | | `res:write` called twice |
| `already_started` | | | x | | `route` / `default_route` after `start()` |
| `message_size` | | | | x | the response is larger than `max_response_bytes` |
| `bad_response` | | | | x | the answer is not an HTTP response |
| `tls_untrusted`, `tls_hostname_mismatch`, `tls_certificate_expired`, `tls_client_certificate_rejected`, `tls_handshake_failed` | | | | x | TLS failure categories of the HTTP client (see `odc.HTTPClient`) |
| `tls_failed` | | x | | | the TLS session failed (the detail says why) |
| `no_tls` | | x | x | x | the build has no TLS support (see below) |
| `tls config: <message>` | | x | x | x | a `tls` table or its files are wrong (naming the key and file, never contents) |
| `invalid_options: <name>` | x | x | x | x | a bad option, argument or serial |

**Availability.** The objects are in every build that has LuaPort. TLS needs OpenSSL in the ODC library (`ODC_ASIO_SSL`) for `odc.TLSSocket`, and HTTPS for the HTTP objects also needs Simple-Web-Server built with TLS (`SIMPLEWEB_SSL=ON`). Without them the constructors that ask for TLS return `nil, "no_tls"` (before looking at the other options) and the plain objects are not affected.

**TLS options: `odc.TLSSocket` against the HTTP objects.**

| | `odc.TLSSocket` | `odc.HTTPServer`, `odc.HTTPClient` |
|---|---|---|
| keys in `tls` | the library's whole `TLS` section: `CertFile`, `KeyFile`, `KeyPasswordEnv`, `CAFile`, `CAPath`, `CRLFile`, `PeerCertAllowList`, `MinVersion`, `MaxVersion`, `Ciphers`, `CipherSuites13`, `Groups`, `SignatureAlgorithms`, `ServerName`, `VerifyPeer`, `VerifyHostname`, `RequireClientCert`, `HandshakeTimeoutms` ... (the names and checks of the port configuration) | only `CertFile`, `KeyFile`, `CAFile`, `VerifyPeer`, `VerifyHostname`; any other key gives `invalid_options: <key> is not supported by the HTTP stack (use odc.TLSSocket)` |
| private key password | `KeyPasswordEnv` (an environment variable); a literal `KeyPassword` is refused | an encrypted key is refused up front (`invalid_options: tls.KeyFile is encrypted ...`): OpenSSL would ask on the console |
| `KeyLogFile` | always refused | not a key of the stack |
| certificate verification | `VerifyPeer` and (client) `VerifyHostname` default to true; false is allowed with a warning on every connection | a client verifies the chain and the host name; an https client needs `CAFile` unless `system_roots = true` |
| failure detail | `on_tls_failed(sock, "tls_failed", detail)` with the library's full text, `s:peer()` | only the coarse categories SWS's error codes and the verify result allow |
| renewed certificate | used by the next connection | the next request (client); a server after `stop()` and `start()` |

**Timing rules.**

* `odc.TCPSocket` / `odc.TLSSocket` client: `connect_timeout_ms` (default 10 s) bounds one connect attempt, and is how a script learns that it failed (why is in the log); `auto_reopen` retries (fixed `retry_ms`, or a back-off starting at 125 ms) until `close()`. `idle_timeout_ms` (default never) ends a connection with no data either way. A TLS handshake has its own deadline, `HandshakeTimeoutms`. **A failed handshake is retried at once** (the manager's back-off restarts whenever a TCP connection comes up): set `retry_ms`, or `close()` in `on_tls_failed`, for a peer whose certificate you will never accept.
* `odc.HTTPClient`: `timeout_ms` (default 15 s) is the whole request, `connect_timeout_ms` (default 5 s) the lookup, connect and handshake; both exact.
* `odc.HTTPServer`: `request_timeout_ms` and `content_timeout_ms` are SWS's, in whole seconds (rounded up); `response_timeout_ms` (default 30 s, exact) answers a handler that stays silent with 504.

**Security notes.**

* Servers listen on `127.0.0.1` unless told otherwise; listening on another address without TLS logs a warning (HTTP and TCP servers). There is no authentication: put a proxy or TLS with client certificates in front of anything reachable from outside.
* **No secrets in logs:** keys, passwords, tokens, request and response bodies and socket data are never logged. At debug level the HTTP objects log method, host, path (without the query string), status and sizes, and no header values (`Authorization`, `Cookie` and the like are never printed).
* A script cannot write the TLS secrets to a file (`KeyLogFile` is refused), cannot give a literal key password, and the HTTP objects refuse encrypted keys and never use the system CA store unless `system_roots = true`.
* TLS clients verify the chain and the host name by default; turning either off logs a warning each time.
* Sizes and times are bounded by defaults (`max_response_bytes` 1 MiB, `max_request_bytes` 1 MiB, the timeouts above); unlimited needs an explicit `0`. What a socket script writes is not bounded: a script that writes faster than its peer reads should keep its own count.
* The HTTP client refuses to build a request that would let a script inject a header or a second request (control characters in the method, path or headers).

**Known limitations.**

* An `odc.TCPSocket` / `odc.TLSSocket` **server takes one connection at a time**; a second client is refused until the first has ended (the HTTP server serves many).
* URLs with IPv6 literals (`http://[::1]:80`) are refused by `odc.HTTPClient`, and its `proxy` option is for `http://` urls only (no `CONNECT`).
* The HTTP client does not follow redirects, keep cookies, reuse connections (one request is one connection, `Connection: close`), retry, decompress, or stream (`text/event-stream`).
* SWS never sends the `413` it means to send for a request over `max_request_bytes`: the connection is closed. An HTTP server answers nothing to a method other than `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD` or `OPTIONS` that no route matches.
* A client certificate that a TLS 1.3 server refuses is seen by the client only as the connection ending (`closed`, or `tls_failed` if the alert arrives first).
* A socket's failed connect attempt (refused, a name that doesn't resolve, unreachable) is not reported as it happens: the attempt ends at `connect_timeout_ms` (`timeout`), and the reason is in the log. A socket server that cannot listen only logs it.
* Tested on Windows (MSVC) and Linux (clang, libc++).

**Examples, and the tests that run them.** The examples are LuaPort scripts. Each example below, and the script in `ExampleConfig/Lua-Network`, is run by a test in `Code/tests/LuaPort_tests`, so it cannot go stale. The blocks that only list options are reference, not scripts.

| Example | Run by |
|---|---|
| `odc.TCPSocket` example (`example:tcp`) | `NetTcpTest.cpp`: "The example in the network objects documentation runs" |
| `odc.TLSSocket` example (`example:tls`) | `NetTlsTest.cpp`: "The example in the network objects documentation runs" |
| `odc.HTTPServer` example (`example:http-server`) | `NetHttpServerTest.cpp`: "The example in the network objects documentation runs" |
| `odc.HTTPClient` example (`example:http-client`) | `NetHttpClientTest.cpp`: "The client example in the network objects documentation runs" |
| `odc.HTTP` example (`example:http-helpers`) | `NetHttpClientTest.cpp`: "The helpers example in the network objects documentation runs" |
| a whole configuration: [`ExampleConfig/Lua-Network`](../../../ExampleConfig/Lua-Network) (a status page and a REST endpoint with `odc.HTTPServer`, a poller with `odc.HTTPClient`) | `NetHttpClientTest.cpp`: "The script in ExampleConfig/Lua-Network ..." (two tests); the configuration was loaded in opendatacon |
| four whole configurations that move ODC events and controls over a transport: [`ExampleConfig/Lua-Transport`](../../../ExampleConfig/Lua-Transport) (`TCP`, `TLS`, `HTTP`, `HTTPS`; see [Moving ODC events over a network](#moving-odc-events-over-a-network)) | `NetExamplesTest.cpp`: the `LuaNetExamplesTestSuite` tests run the real scripts; each configuration was also run in opendatacon |

Also see `ExampleConfig/SelfDocumentingLuaPort/LuaPortDoco.lua` for the rest of the `odc` table, and `Code/tests/LuaPort_tests` for what each object does in the corner cases.

## `odc.TCPSocket`

A TCP client or server (one `TCPSessionManager`). Nothing is opened until `:open()`.

```lua
local s, err = odc.TCPSocket{
  host = "192.0.2.10",           -- client: the host to connect to (required). server: the address to bind to (default 127.0.0.1)
  port = 7000,                   -- required, 1 to 65535 (an integer, or a string of digits)
  server = false,                -- true: listen. One connection at a time (see below)
  auto_reopen = false,           -- keep connecting (or listening) again until :close()
  retry_ms = 0,                  -- fixed time between attempts when auto_reopen (up to 65535); 0 = exponential backoff, 125 ms doubling
  connect_timeout_ms = 10000,    -- client: give up on a connect attempt after this long (0 = never)
  idle_timeout_ms = 0,           -- end a connection with no data in either direction for this long (0 = never)
  write_buffer_limit = 0,        -- the number of queued writes (0 = no limit); beyond it the manager DROPS the oldest
  throttle_bitrate = 0, throttle_chunksize = 0, throttle_writedelay_ms = 0,
  keepalive = true, keepalive_idle_s = 599, keepalive_retry_s = 10, keepalive_count = 3,
  on_open  = function(sock) end,                          -- a connection is up
  on_data  = function(sock, data) end,                    -- bytes arrived (a binary-safe string, any length, any split)
  on_close = function(sock, reason, detail) end,          -- a connection ended, or the object ended without one (see below)
  on_error = function(sock, reason, detail) end,          -- a connect attempt timed out and will be retried (auto_reopen)
}
```

The constructor returns the object, or `nil, "invalid_options: <option>"` (the first option that is not valid; an option it does not know is invalid too, so a typo is not ignored). Numbers are non-negative integers (a float with an integer value is accepted); the ranges are: timeouts up to 2^31-1 ms, `keepalive_idle_s` up to 604800, `keepalive_retry_s` up to 86400, `keepalive_count` up to 255, the other limits and throttle values up to 2^32-1; `host` is a non-empty string of at most 255 bytes; callbacks are functions or nil. Callbacks can also be set, replaced or (with `nil`) removed with `s:on("open" | "data" | "close" | "error", fn)` (the names `"on_open"` ... are accepted too).

| Method | |
|---|---|
| `s:open()` | start connecting or listening. `true`, or `nil, "closed"` after `:close()` |
| `s:write(data)` | queue data (a string; a number is converted, any other type raises a Lua error; an empty string is accepted and sends nothing). `true`, or `nil, "closed"` (not open, no connection up, or closing): a write is never kept for a later connection |
| `s:write_to(serial, data)` | like `write`, but only if `serial` is the current connection, else `nil, "closed"`: safe across reconnects |
| `s:close()` | stop, and end the current connection once what was written has been sent. `true`. Idempotent |
| `s:is_open()` | `true` while a connection is up |
| `s:serial()` | the number of the current connection (1, 2, 3 ... changing on each reconnect), 0 when there is none |
| `s:on(event, fn)` | set a callback. `true`, or `nil, "invalid_options: <event>"`, or `nil, "closed"` once the object is closing or closed |

### Callbacks

* They run on the script's strand, one at a time, in order, **never from inside the call that triggered them** (`on_close` after `close()` is called after `close()` has returned). An error raised in a callback is logged, with the port name, and does not stop the others.
* `on_open`: a connection came up (`sock:serial()` is its number).
* `on_data`: data exactly as read; there is no framing. Framing (lines, lengths, HTTP) is the script's job.
* `on_close(sock, reason, detail)`, **once per connection** that came up, when it ends: `reason` is `"closed"` (the peer or `close()` ended it) or one of the strings below; `detail` is the system's message where there is one. It is also called once when the object ends **without** a connection: a connect attempt that did not succeed within `connect_timeout_ms`, with `auto_reopen = false` (`"timeout"`; why it failed is in the log). Without `auto_reopen` the object is finished after that. With `auto_reopen` it connects again and calls `on_open` for the next connection.
* `on_error(sock, reason, detail)`: with `auto_reopen`, a connect attempt did not succeed within `connect_timeout_ms` (`"timeout"`), and a new one is started. Without `auto_reopen` it is reported by `on_close`. `:close()` with no connection up (waiting to retry) calls nothing.
* When the host is **destroyed** every object still open is torn down and **no callback is made**, not even `on_close`. Nothing is closed for you otherwise: `close()` your objects where the host lets you (a port's or transform's `Disable()`, a UI script's `Cancel()`); an `on_close` then follows as usual, after that call has returned.

### Error strings

| String | Meaning |
|---|---|
| `"timeout"` | `connect_timeout_ms` passed (detail `connect_timeout_ms`: a connect attempt did not succeed, the log says why), or `idle_timeout_ms` passed (detail `idle_timeout_ms`) |
| `"closed"` | the connection ended: the peer closed it, it was lost, or `close()` ended it |
| `"invalid_options: <name>"` | a constructor option, a `write_to` serial, or an `on` event name that is not valid |

The socket layer logs why a connect attempt failed or a connection ended (refused, reset, no route ...), with the host's name and `host:port`.

### Things to know

* **A server takes one connection at a time.** A second client is refused until the first has ended. For several simultaneous clients, open several listeners.
* **`connect_timeout_ms`** covers the name resolution and the connect of one attempt (an attempt that fails at once, refused say, still ends at the timeout: set it short for a quick answer). With `auto_reopen` a new attempt is started when it passes. It does not apply to a server: a server that cannot listen logs it, and with `auto_reopen` keeps trying.
* **`idle_timeout_ms`** is measured from the last byte read or written (by `write` calls: when they were accepted).
* **A host name can resolve to several addresses**; they are tried at once, and the first that connects is used (so `"localhost"` works when one of its addresses refuses).
* **Writes** are not bounded by the object; `write_buffer_limit` is the manager's own limit and drops the oldest queued write when it is exceeded: leave it at 0 unless that is what you want. A write is never kept for a later connection.
* **Lifetime.** An open object keeps itself alive: collecting the last Lua reference to it does **not** close it. It ends with `:close()`, or with a failure it does not recover from (no `auto_reopen`); `close()` it (see [The Lua hosts](#the-lua-hosts)): nothing does it for you. A closed or never opened object that is collected is destroyed.
* **Logging.** The socket layer's lines (connect attempts, errors) go to the host's log with its name and `host:port`. Data is never logged. A server on a non-loopback address without TLS logs a warning when it is opened.

### Example

The example below is run by the tests (`Code/tests/LuaPort_tests`), so it cannot go stale. Use it as the `LuaFile` of a Lua port (in another host, the same calls go in that host's own functions).

<!-- example:tcp -->
```lua
local PORT = odc.Config.Port or 7000

function Build() end
function Event() end

function Enable()
	-- a server that echoes what it is sent
	server = odc.TCPSocket{ host = "127.0.0.1", port = PORT, server = true, auto_reopen = true,
		on_data = function(sock, data) sock:write(data) end }
	assert(server:open())

	-- a client that sends a greeting, logs the echo, and closes
	client = odc.TCPSocket{ host = "127.0.0.1", port = PORT, auto_reopen = true, retry_ms = 100,
		on_open  = function(sock) sock:write("hello") end,
		on_data  = function(sock, data) odc.log.info("client received: "..data); sock:close() end,
		on_close = function(sock, reason) odc.log.info("client closed: "..reason) end,
		on_error = function(sock, reason, detail) odc.log.info("client could not connect yet: "..reason) end }
	assert(client:open())
end

-- close what Enable() opened: the port does not do it
function Disable()
	client:close()
	server:close()
end
```

## `odc.TLSSocket`

The same object with TLS on top (one `TCPSessionManager` with TLS, 1.2 and 1.3, as in IEC 62351-3): the TCP client is the TLS client and the TCP listener the TLS server. Everything above applies (options, methods, `on_data`, `on_error`, `write_to`, timeouts, lifetime), with these differences. Without TLS support in the build (no OpenSSL) the constructor returns `nil, "no_tls"`.

```lua
local s, err = odc.TLSSocket{
  host = "api.example.test", port = 8443,
  server = false,                  -- the TLS role follows it
  peer_name = "api.example.test",  -- client: the name checked against the certificate, and sent as SNI if it is a host name. Default: tls.ServerName, else host
  tls = {                          -- the "TLS" section of a port configuration, with the library's key names and checks
    CAFile = "/etc/odc/ca.pem",                                  -- the CAs the peer's certificate must chain to
    CertFile = "/etc/odc/client.pem", KeyFile = "/etc/odc/client.key", KeyPasswordEnv = "ODC_CLIENT_KEY_PW",   -- ours (mutual TLS)
    MinVersion = "1.2", HandshakeTimeoutms = 10000,              -- (a version is a string: "1.2" or "1.3")
  },
  on_open       = function(sock, peer) end,                      -- the TLS session is established
  on_tls_failed = function(sock, reason, detail) end,            -- the session failed: see below
  on_data = ..., on_close = ..., on_error = ...,                 -- as for odc.TCPSocket
}
```

* **`tls`** is required and is checked when the object is made: a problem returns `nil, "tls config: <message naming the key>"`, and a certificate, key or CA file that cannot be read or does not fit returns `nil, <message naming the key and the file>` (never the contents). `"Enabled"` is forced on. A literal `KeyPassword` is refused (put the password in an environment variable and name it with `KeyPasswordEnv`). `KeyLogFile` is always refused: `nil, "invalid_options: tls.KeyLogFile"`.
* **Safe defaults.** The peer's certificate is verified (`VerifyPeer`), and a client also checks that the certificate carries the name it connected to (`VerifyHostname` is true unless the table says otherwise; with an IP address as `host` the certificate needs an IP subject alternative name). Setting either to false is allowed, and a warning is logged on every connection.
* **`on_tls_failed`** can be set with `s:on("tls_failed", fn)` too.
* `peer_name` and `tls` exist only for TLS objects.
* **What the script sees.** For each TCP connection that comes up:

  | What happened | Callbacks |
  |---|---|
  | the TCP connect does not succeed in `connect_timeout_ms` | as for `odc.TCPSocket`: `on_error` with `auto_reopen`, else `on_close` with `"timeout"`. No `on_tls_failed`, no `on_open` |
  | the handshake fails (untrusted or expired certificate, wrong name, a client certificate refused, no common protocol, garbage), or does not complete in `HandshakeTimeoutms` | `on_tls_failed(sock, "tls_failed", detail)`, then `on_close(sock, "tls_failed", detail)`. No `on_open` |
  | the session is established | `on_open(sock, peer)`; the connection's `on_close` follows when it ends |
  | the peer closes the session (close_notify) | `on_close(sock, "closed")` |
  | the TCP connection is lost | `on_close(sock, "closed")` (the log says why) |
  | the session fails after it was established (an alert, bad records) | `on_tls_failed`, then `on_close` with `"tls_failed"` |
  | `close()`, `idle_timeout_ms` | close_notify is sent first. `on_close(sock, "closed")` (idle: `"timeout"`) |

  `on_close` is called once for each TCP connection that came up, whether or not its session was ever established. Without `auto_reopen` the object ends with its connection, as `odc.TCPSocket` does. With it a failed handshake is retried at once, or after `retry_ms` if you set it: set `retry_ms` for a client that may meet a certificate it will never accept.
* **`on_open(sock, peer)`** gets `peer`, a table: `protocol` ("TLSv1.3"), `cipher`, `resumed`, `certificate` (the peer presented one), `subject` (RFC 2253 form, e.g. `CN=api.example.test`), `issuer`, `serial` (hex), `sha256` (the certificate's fingerprint), `not_before`, `not_after` (ISO 8601 UTC) and `seconds_to_expiry`. `s:peer()` returns the same table for the last session (`nil` if there has been none).
* **`write`.** The TCP connection takes writes as soon as it is up: data written before the handshake completes waits for it and is sent in order. Once the session has ended (failed, closed) or there is no connection, `write` returns `nil, "closed"` and nothing is carried to a later connection. `s:serial()` is the number of the TCP connection, from when it is up; `s:is_open()` is true when the session is established.
* **`close()`** sends close_notify after what was written. A **renewed certificate** (a changed `CertFile`, `KeyFile`, `CAFile` or CRL on disk) is used by the next connection, without a restart.
* **`connect_timeout_ms`** covers the TCP connect only; the handshake has its own timeout, `HandshakeTimeoutms`.

### TLS failures

`on_tls_failed(sock, reason, detail)` and the `on_close` that follows get `"tls_failed"`, and the library's description of the failure as the detail: what OpenSSL reported, the result of the certificate verification (`certificate verification failed: ...`), the alert sent or received, and a summary of the handshake as far as it got (`we are the TLS client; sent: ClientHello (1522 bytes); received: ...`). The detail is for the log, not to compare.

With TLS 1.3 a client whose certificate the server refuses has completed its own handshake first, so it sees `on_open`, and then the connection ends: `on_close` (`"closed"`, or `"tls_failed"` if the server's alert arrives first). The server's side reports `on_tls_failed`.

### Example

Run by the tests with a test CA (the file names come from the port's configuration, `odc.Config`).

<!-- example:tls -->
```lua
local C = odc.Config
local PORT = C.Port or 7443

function Build() end
function Event() end

function Enable()
	-- an echo server that wants a client certificate from the same CA
	server = odc.TLSSocket{ host = "127.0.0.1", port = PORT, server = true, auto_reopen = true,
		tls = { CertFile = C.ServerCert, KeyFile = C.ServerKey, CAFile = C.CAFile },
		on_data = function(sock, data) sock:write(data) end }
	assert(server:open())

	-- a client that checks the server's certificate and name, says hello, logs the echo, and closes
	client = odc.TLSSocket{ host = "127.0.0.1", port = PORT, auto_reopen = true, retry_ms = 100,
		tls = { CertFile = C.ClientCert, KeyFile = C.ClientKey, CAFile = C.CAFile },
		on_open  = function(sock, peer) odc.log.info("client connected to "..peer.subject); sock:write("hello") end,
		on_data  = function(sock, data) odc.log.info("client received: "..data); sock:close() end,
		on_tls_failed = function(sock, reason, detail) odc.log.error("client TLS failure: "..reason.." "..detail) end,
		on_close = function(sock, reason) odc.log.info("client closed: "..reason) end }
	assert(client:open())
end

function Disable()
	client:close()
	server:close()
end
```

## `odc.HTTPServer`

An HTTP or HTTPS server on Simple-Web-Server, running on ODC's own threads: a request is read and answered without a thread of its own, and **many connections are served at once** (unlike `odc.TCPSocket` in server mode). Your handlers always run on the port's strand, one at a time, like every other callback. There is no authentication, and no TLS unless you ask for it: a server is for the loopback interface or a trusted network, behind a proxy for anything else.

```lua
local srv, err = odc.HTTPServer{
  address = "127.0.0.1",      -- an IP address (default loopback: the server is never reachable from outside unless you say so)
  port = 8080,                -- 0 = pick one (default): s:port() says which after s:start()
  tls = nil,                  -- https: { CertFile = ..., KeyFile = ..., CAFile = ..., VerifyPeer = true }: see below
  request_timeout_ms = 5000,  -- time allowed for the request line and headers
  content_timeout_ms = 60000, -- time allowed for the body, and for sending the answer
  max_request_bytes = 1048576,-- the largest request header, and the largest body (a bigger one never reaches the script: SWS closes the connection, it never sends the 413 it means to)
  response_timeout_ms = 30000,-- a handler that has not answered by then is answered 504 (0 = no limit)
}
```

Returns the object, or `nil, "invalid_options: <name>"` (an option that is not known is invalid, and so is an address that is not an IP address: no host names). `request_timeout_ms` and `content_timeout_ms` are rounded up to whole seconds (the granularity of Simple-Web-Server) and 0 means no limit; `response_timeout_ms` is exact. Numbers are non-negative integers up to 2^31-1 (`port` up to 65535, `max_request_bytes` from 1 to 2^32-1).

| Method | |
|---|---|
| `srv:route(method, pattern, fn)` | `fn(req, res)` is called for a request whose method is `method` (upper case, `"GET"`) and whose path matches `pattern`: an **ECMAScript regular expression** (not a Lua pattern, so a digit is `"\\d"` in a Lua string) that must match the whole path. If several match, the one that sorts first by its text is used. Registering the same method and pattern again replaces the function. `true`, or `nil, "invalid_options: method" / "pattern" / "handler"`, or `nil, "already_started"` |
| `srv:default_route(fn)` | `fn(req, res)` for a request no route matches (for `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD` and `OPTIONS`; a request with another method that no route matches gets no answer from SWS). Without one the answer is a plain `404 Not Found`. `true`, or `nil, "invalid_options: handler"`, or `nil, "already_started"` |
| `srv:start()` | listen. `true`, or `nil, "address_in_use" / "bind_failed", detail`, or `nil, "tls config: ..."`. Routes are fixed once it has started; after `stop()` they can be changed and it can be started again |
| `srv:stop()` | close the listener and every connection. Requests that have not been answered are dropped. `true`. Idempotent |
| `srv:port()` | the port it listens on (0 before the first start) |

The request `req`: `method`, `path` (percent-decoded), `query_string` (as sent), `query` (a table: the decoded names and values; a repeated name gives the last value), `query_all` (a table of lists, for repeated names), `http_version`, `headers` (a table with lower-case names; a header sent twice is joined with `", "`), `body` (a string), `captures` (an array: the groups of the route's regular expression, empty string for a group that did not take part), `remote_addr` and `remote_port`.

The response `res`:

* **`res:write(status, body, headers)`** sends the answer, **once**: `status` is a number that is a known HTTP status (200, 404 ...), `body` a string (or nothing), `headers` an optional table of names and values (strings or numbers). It returns `true`, or `nil, "already_written"` (the second call: the first answer stands; also after the 504 backstop), `nil, "closed"` (the server was stopped), or `nil, "invalid_options"` (a status that is not known, a header name that is not a token or a value with a line break, a `Content-Length` or `Transfer-Encoding` header, a body that is not a string). `Content-Length` is set for you.
* **`res:close_after()`** closes the connection after this answer (and says `Connection: close`).
* **An answer may come later**: keep `res` (or use it in a timer callback of `odc.msTimerCallback`, or in the callback of another object) and call `res:write()` then. A handler that has not answered within `response_timeout_ms` is answered `504 Gateway Timeout`, and a warning is logged, so a script that forgets cannot leave a client hanging.
* **A handler that raises an error** is logged (with the host's name) and the client gets `500 Internal Server Error`, without the text of the error; if it had already written, the answer stands. The other routes are not affected.

**HTTPS.** `tls = { CertFile = ..., KeyFile = ..., CAFile = ..., VerifyPeer = true }` makes an https server (TLS 1.2 or later). The Simple-Web-Server stack takes only these keys (`VerifyHostname` is accepted and means nothing for a server): **any other key is refused**, `nil, "invalid_options: <key> is not supported by the HTTP stack (use odc.TLSSocket)"`, and so are files that cannot be read (`invalid_options: tls.CertFile cannot be read`) and **a private key with a password** (`invalid_options: tls.KeyFile is encrypted ...`: it would make OpenSSL ask on the console). With a `CAFile` the server asks every client for a certificate that chains to it and refuses a client without one (mutual TLS); with `VerifyPeer = false` the `CAFile` is not used for that. A certificate and key that do not belong together are reported by `start()` (`nil, "tls config: ..."`). Without TLS support in the build (no OpenSSL, or Simple-Web-Server built without `SIMPLEWEB_SSL`) a `tls` option gives `nil, "no_tls"`. A renewed certificate is used by a server that is stopped and started again.

**Lifetime and logging.** A started server keeps itself alive: collecting the last Lua reference to it does **not** stop it. It ends with `srv:stop()`: do that yourself (see [The Lua hosts](#the-lua-hosts)). When the host is destroyed a server still started is stopped without calling any handler. A server that was never started and is collected is destroyed. A server on a non-loopback address without TLS logs a warning when it starts. At debug level the method, path, status and size of each answer are logged: never a body or a header value.

### HTTP example

A status page and a JSON endpoint. Run by the tests with `odc.Config.Port` set to a free port.

<!-- example:http-server -->
```lua
local PORT = odc.Config.Port or 8080
local counter = 0
local JSON = { ["Content-Type"] = "application/json" }

function Build() end
function Event() end

function Enable()
	server = odc.HTTPServer{ port = PORT }

	-- a status page
	assert(server:route("GET", "^/$", function(req, res)
		res:write(200, "<html><body><h1>opendatacon: "..odc.Name.."</h1><p>counter: "..counter.."</p></body></html>",
			{ ["Content-Type"] = "text/html; charset=utf-8" })
	end))

	-- a REST endpoint: GET the counter, POST {"add": n} to change it
	assert(server:route("GET", "^/api/counter$", function(req, res)
		res:write(200, odc.EncodeJSON({ counter = counter }), JSON)
	end))
	assert(server:route("POST", "^/api/counter$", function(req, res)
		local body = odc.DecodeJSON(req.body)
		if type(body) ~= "table" or type(body.add) ~= "number" then
			res:write(400, odc.EncodeJSON({ error = "expected {\"add\": number}" }), JSON)
			return
		end
		counter = counter + body.add
		res:write(200, odc.EncodeJSON({ counter = counter }), JSON)
	end))

	assert(server:default_route(function(req, res) res:write(404, "no such page: "..req.path) end))
	assert(server:start())
	odc.log.info("listening on port "..server:port())
end

-- stop what Enable() started: the port does not do it
function Disable()
	server:stop()
end
```

## `odc.HTTPClient`

An HTTP or HTTPS client on Simple-Web-Server, running on ODC's own threads: a request is made and answered without a thread of its own, and **any number of requests can be in flight at once**, on one client or on many. The answer is given to a callback on the port's strand, like every other callback; it is never called from inside the `request()` that started it.

```lua
local c, err = odc.HTTPClient{
  url = "https://api.example.test:8443",  -- http:// or https://, a host name or an IPv4 address, an optional port; no path (it is given per request)
  timeout_ms = 15000,          -- the whole request, from the call to the end of the response (0 = no limit)
  connect_timeout_ms = 5000,   -- the name lookup, the connection and the TLS handshake together (0 = no limit)
  max_response_bytes = 1048576,-- the largest response body (0 = no limit): a bigger one fails with "message_size"
  proxy = nil,                 -- "host:port" of an HTTP proxy (http:// urls only)
  tls = { CAFile = "ca.pem" }, -- https only: see below
  system_roots = false,        -- https only: also trust the operating system's CA store
}
```

Returns the object, or `nil, "invalid_options: <name>"` (an option that is not known is invalid, and so is an IPv6 literal in the url: use a name), or `nil, "no_tls"` (an https url in a build without TLS), or `nil, "tls config: ...", detail` (a certificate and key that do not belong together, a CA file that is not a PEM). Nothing is connected until the first request.

| Method | |
|---|---|
| `c:request{ method, path, headers, body, on_response }` | makes a request and returns `true` at once, or `nil, "invalid_options: <name>"` (nothing is sent and `on_response` is not called), `nil, "closed"` (after `c:close()`), `nil, "aborted"` (the port is closing). `method` is upper case (`"GET"`, `"POST"` ...); `path` starts with `/` and may have a query (build one with `odc.HTTP.query_encode`; default `"/"`); `headers` is an optional table of names and values (strings or numbers); `body` an optional string; `on_response(resp, err, detail)` is called **exactly once** |
| `c:cancel()` | aborts every request in flight: each `on_response` is called with `nil, "aborted"` once `cancel()` has returned. The client stays usable. `true` |
| `c:close()` | like `cancel()`, and the client refuses new requests (`nil, "closed"`). `true`. Idempotent |

**The result.** On any HTTP status (a 404 or a 500 is a normal answer) `on_response(resp)` is called with `err == nil` and
`resp = { status = 200, reason = "OK", http_version = "1.1", headers = { ["content-type"] = "application/json", ... }, body = "..." }`. Header names are lower case, a header sent more than once is joined with `", "`, except `set-cookie`, which is always a list of the values (`resp.headers["set-cookie"][1]`). `reason` is the server's text. On a transport error `on_response(nil, err, detail)` is called: `err` is one of the strings below and `detail` the system's or the library's text (to log, never to compare).

| `err` | Meaning |
|---|---|
| `"resolve_failed"` | the host name could not be looked up |
| `"refused"` | the connection was refused |
| `"unreachable"` | no route to the host or network |
| `"timeout"` | `timeout_ms` passed without a complete response, or `connect_timeout_ms` passed before the connection (and the handshake) was up (`detail` says which) |
| `"closed"` | the server closed the connection or reset it before the response was complete (no response, or a body shorter than its `Content-Length`) |
| `"aborted"` | `c:cancel()` or `c:close()` |
| `"message_size"` | the response (its headers, or its body by `Content-Length`, in chunks, or until the connection closed) is larger than `max_response_bytes` |
| `"bad_response"` | not an HTTP response: no status line, a `Content-Length` that is not a number (or two that differ), a chunk size that is not hex |
| `"tls_untrusted"`, `"tls_hostname_mismatch"`, `"tls_certificate_expired"` | the server's certificate was not accepted: its chain does not lead to the `CAFile` (or the system roots), its name does not match the host of the url, or it is outside its validity period. These come from the result of OpenSSL's verification of the chain, not from text |
| `"tls_client_certificate_rejected"` | the server sent an alert that says it refused our certificate (`bad_certificate`, `unknown_ca`, `certificate_expired`, `certificate_revoked`, `certificate_required` ...) |
| `"tls_handshake_failed"` | any other TLS failure: no common protocol or cipher, the server is not TLS at all, the handshake was broken off |
| `"failed"` | anything else (the system's text is the detail) |

**What cannot be told apart.** Simple-Web-Server reports an `asio` error code and no more, so: a TLS failure is classified from the verification result and the alert the server sent, and a server that simply closes the connection instead of sending an alert is `"closed"` or `"tls_handshake_failed"`, not a certificate error. A server that wants a client certificate and gets none is `"tls_client_certificate_rejected"` with TLS 1.3 (the client's handshake is complete before the server looks at its certificate, and the alert arrives with the first read), but the server closes the connection at once and if the reset reaches the client before the alert (it does, more often on a busy machine, and on Windows a reset throws away what was received and not read) all the client knows is `"closed"`; with TLS 1.2 the server sends a plain `handshake_failure`, which is `"tls_handshake_failed"`. Treat the three as "the server did not accept the client certificate" and look at the server's log. A failure while resolving the name is `"resolve_failed"` whatever the resolver said (no network, a name that does not exist). If the server answers a request and then something goes wrong with the TLS shutdown, you get the answer. `odc.TLSSocket` reports more detail, if you need it.

**What the binding does and does not do.**

* One request is one connection: a **fresh Simple-Web-Server client is made for every request**, with `Connection: close`. There is no connection reuse, no pool and no retry (a request is never sent twice by the binding; Simple-Web-Server would, once, and that is switched off). Requests on one `odc.HTTPClient` run independently and may overlap; their callbacks come in the order the answers arrive.
* **Redirects are not followed** (a 302 is a normal answer with a `location` header), there are no cookies, no automatic decompression (set `Accept-Encoding` only if you decode the body yourself), and a streaming response (`text/event-stream`, a response without a length that never ends) is not supported: a response without `Content-Length` and without chunking ends when the server closes the connection.
* The binding sets `Host`, `Connection`, `Content-Length` and `Transfer-Encoding`; a script that sets one of them, or `Expect` or `Upgrade`, gets `nil, "invalid_options: headers.<name> is set by the binding"`. A method that is not a token, a path with a space or a control character, a header name that is not a token and a header value with a control character (CR, LF, NUL ...) are refused the same way (`invalid_options: method` / `path` / `headers...`): a script cannot inject a header or a second request. The `Content-Length` of the response is checked, and what is beyond it is ignored.
* A response to `HEAD`, and any `1xx`, `204` and `304` response, has no body, whatever its `Content-Length` says.
* `timeout_ms` is exact (a timer of ODC's, not Simple-Web-Server's whole seconds) and covers everything from the call to the last byte of the answer; `connect_timeout_ms` covers the name lookup, the connection and the TLS handshake. Both give `"timeout"`.
* `proxy = "host:port"` sends the request to an HTTP proxy as a request for the absolute URL (`GET http://host/path`). It is for `http://` urls: with an https url it is refused (`invalid_options: proxy`), as the binding does not implement `CONNECT` tunnels. Proxy authentication is not supported (set a `Proxy-Authorization` header yourself if needed).
* At debug level the method, host, path (without the query string, which can carry a token), status and sizes are logged: never a body or a header value. Headers are never logged by the binding.

**HTTPS.** `tls = { CAFile = ..., VerifyPeer = true, VerifyHostname = true, CertFile = ..., KeyFile = ... }`: TLS 1.2 or later, the chain and the host name of the server are verified, with SNI. The Simple-Web-Server stack takes only these keys: **any other key is refused**, `nil, "invalid_options: <key> is not supported by the HTTP stack (use odc.TLSSocket)"` (no key password, CRL, allow list, version or cipher settings, key log or tracing: use `odc.TLSSocket` and your own HTTP for those), and so are files that cannot be read (`invalid_options: tls.CertFile cannot be read`) and **a private key with a password** (`invalid_options: tls.KeyFile is encrypted ...`: it would make OpenSSL ask on the console). `CertFile` and `KeyFile` go together and make the client present a certificate (mutual TLS).

* **`CAFile` is required** for an https url (`nil, "invalid_options: tls.CAFile (required for https unless system_roots = true)"`), unless you set **`system_roots = true`**, which also trusts the operating system's CA store (use it only for a server with a public certificate: the store is never used by accident). `VerifyPeer = false` or `VerifyHostname = false` are allowed (for a test system) and log a warning on every request.
* `VerifyHostname` is on by default: the certificate must name the host of the url (an IP address in the url must be in its subject alternative names).
* **A renewed certificate is used by the next request**: the files are read for each request (every request has its own client), so a client certificate, key or CA file that was replaced on disk is used without making a new `odc.HTTPClient`. A bad file found then is reported by `request()` as `nil, "tls config: ...", detail`.
* Without TLS support in the build (no OpenSSL, or Simple-Web-Server built without `SIMPLEWEB_SSL`) an https url gives `nil, "no_tls"`; an http url is not affected.

**Lifetime.** An `odc.HTTPClient` with requests in flight keeps itself alive: collecting the last Lua reference does **not** cancel them (their callbacks are still called), and a client with nothing in flight that is collected is destroyed. When the host is destroyed every request in flight is dropped **without calling its callback**, and the connections are closed. Nothing is cancelled for you otherwise: `close()` or `cancel()` the client yourself (each `on_response` then gets `nil, "aborted"`). An error raised in `on_response` is logged with the host's name and does not stop other callbacks.

### HTTP client example

A JSON POST to a server in the same script (any HTTP server would do). Run by the tests with `odc.Config.Port` set to a free port.

<!-- example:http-client -->
```lua
local PORT = odc.Config.Port or 8080
local JSON = { ["Content-Type"] = "application/json" }

function Build() end
function Event() end

function Enable()
	-- a small server to talk to
	local counter = 0
	server = odc.HTTPServer{ port = PORT }
	assert(server:route("POST", "^/api/counter$", function(req, res)
		counter = counter + odc.DecodeJSON(req.body).add
		res:write(200, odc.EncodeJSON({ counter = counter }), JSON)
	end))
	assert(server:start())

	client = assert(odc.HTTPClient{ url = "http://127.0.0.1:"..server:port(), timeout_ms = 5000 })
	local function add(n, after)
		local ok, err = client:request{
			method = "POST",
			path = "/api/counter?"..odc.HTTP.query_encode{ source = "example" },
			headers = { ["Content-Type"] = "application/json", Accept = "application/json" },
			body = odc.EncodeJSON({ add = n }),
			on_response = function(resp, err, detail)
				if err then
					odc.log.error("request failed: "..err..(detail and " ("..detail..")" or ""))
					return
				end
				if resp.status ~= 200 then
					odc.log.warning("unexpected status "..resp.status.." "..resp.reason)
					return
				end
				odc.log.info("counter is now "..odc.DecodeJSON(resp.body).counter)
				if after then after() end
			end,
		}
		if not ok then odc.log.error("could not make the request: "..err) end
	end
	add(5, function() add(2) end)
end

function Disable()
	client:close()
	server:stop()
end
```

## `odc.HTTP`

Pure helper functions (no network, no state) for building and reading HTTP things. They are available wherever `odc.HTTPClient` and `odc.HTTPServer` are.

| Function | |
|---|---|
| `odc.HTTP.percent_encode(s)` | percent-encodes every byte except letters, digits and `-` `.` `_` `~` (so a space is `%20`) |
| `odc.HTTP.percent_decode(s)` | decodes `%XX` sequences; a `%` not followed by two hex digits is kept as it is, a `+` stays a `+` |
| `odc.HTTP.query_encode(t)` | `{ a = "1", b = "x y" }` gives `"a=1&b=x%20y"`: the keys sorted (by byte), the names and values percent-encoded. A value may be a string, a number, a boolean (`"true"` / `"false"`) or a list of those, which repeats the key (`{ tag = { "x", "y" } }` gives `tag=x&tag=y`). `nil, "invalid_options: ..."` for anything else |
| `odc.HTTP.query_parse(s)` | `"a=1&b=x%20y"` (a leading `?` is allowed) gives `{ a = "1", b = "x y" }` (a repeated key gives the last value; `+` is a space; a key without `=` has the value `""`) and, as a second result, a table of lists with all the values of each key |
| `odc.HTTP.status` | the status names to numbers, from the Simple-Web-Server list: `odc.HTTP.status.ok == 200`, `.created`, `.no_content`, `.not_found`, `.internal_server_error` ... (the reason phrase in lower case with `_`) |
| `odc.HTTP.reason(code)` | the standard reason phrase: `odc.HTTP.reason(404) == "Not Found"`; `nil` for a number that is not a known status |
| `odc.HTTP.date([ms])` | an RFC 7231 date of a time in milliseconds since 1970 (now if none), in UTC: `odc.HTTP.date(0) == "Thu, 01 Jan 1970 00:00:00 GMT"`. `nil, "invalid_options: ms"` for a negative time or one after the year 9999 |

Each returns `nil, "invalid_options: <argument>"` if given something of the wrong type.

### HTTP helpers example

Run by the tests.

<!-- example:http-helpers -->
```lua
function Build() end
function Event() end

function Enable()
	local q = odc.HTTP.query_encode{ name = "a b&c", limit = 10, tags = { "x", "y" } }
	odc.log.info("query: "..q)
	local parsed, all = odc.HTTP.query_parse(q)
	odc.log.info("name: "..parsed.name.." tags: "..#all.tags)
	odc.log.info("encoded: "..odc.HTTP.percent_encode("100% of a/b"))
	odc.log.info("decoded: "..odc.HTTP.percent_decode("100%25%20of%20a%2Fb"))
	odc.log.info("status: "..odc.HTTP.status.not_found.." "..odc.HTTP.reason(404))
	odc.log.info("date: "..odc.HTTP.date(784111777000))
end

function Disable() end
```

## Moving ODC events over a network

`ExampleConfig/Lua-Transport` holds four runnable configurations (`TCP`, `TLS`, `HTTP`, `HTTPS`), each one opendatacon process on loopback with four Lua ports: `Source` (generates every event type and answers controls) -> `Sender` ==transport==> `Receiver` -> `Sink` (logs the events and issues six controls). The Sender's `Event()` serialises what it is given and sends it with the objects above, the Receiver deserialises and publishes it with `odc.PublishEvent`, and a control issued at the Sink goes back the same way, its `CommandStatus` returned to the originator through the status callback of `Event()`. That is the pattern for any ODC-to-ODC or ODC-to-something link written in Lua. The [example README](../../../ExampleConfig/Lua-Transport/README.md) has how to run each one (the TLS and HTTPS ones need `make-certs` first), what the log shows, and the wire format.

| | Sender | Receiver | Framing |
|---|---|---|---|
| `TCP` | `odc.TCPSocket` client, `auto_reopen` | `odc.TCPSocket` server (one connection at a time) | one JSON message per line |
| `TLS` | `odc.TLSSocket` client, mutual TLS | `odc.TLSSocket` server, client certificate required | one JSON message per line |
| `HTTP` | `odc.HTTPClient` for `POST /events` batches, `odc.HTTPServer` for `POST /control` | `odc.HTTPServer` for `/events`, `odc.HTTPClient` for controls | a JSON body each way, status 200 as the acknowledgement |
| `HTTPS` | the same with the restricted HTTP `tls` table (`CertFile`, `KeyFile`, `CAFile`) and mutual TLS | the same | the same |

**Wire format.** `odc_wire.lua` (shared by all four, loaded with `require`) turns an event into `{ type = <odc.ToString.EventType name>, index, quality (the QualityFlags number), time (ms since 1970), payload }` inside a message `{ v = 1, kind = "event" | "events" | "control" | "status" | "ack", ... }`:

| Event type | Payload on the wire |
|---|---|
| Binary, BinaryOutputStatus | boolean |
| Analog, AnalogOutputStatus | finite number |
| Counter, FrozenCounter | integer 0 to 4294967295 |
| DoubleBitBinary | `{ first, second }` |
| OctetString | hex string |
| ConnectState | name |
| ControlRelayOutputBlock | `{ code (name), count, on_ms, off_ms, status (name) }` |
| AnalogOutputInt16, Int32, Float32, Double64 | `{ value, status (name) }` |

A message that is not understood (bad JSON, unknown version, unknown type, wrong payload) is logged as an error and dropped; the stream carries on.

**Controls.** A control is sent as `{ kind = "control", id = n, event = ... }`; the answer is `{ kind = "status", id = n, status = "SUCCESS" }`. The Receiver holds the status callback until the answer arrives: it gets `TIMEOUT` after `ControlTimeoutMs` (default 5 s) with no answer, `DOWNSTREAM_FAIL` if the transport is down. Over TCP and TLS the control and the answer travel on the connection the Sender opened; over HTTP and HTTPS each side runs a client and a server and the Receiver `POST`s the control to the Sender's `/control`.

**Tests.** `NetExamplesTest.cpp` runs the real scripts: every type round trips in order with the bytes split and joined and across a reconnect, the statuses (`SUCCESS`, other statuses, `TIMEOUT`, `DOWNSTREAM_FAIL`) come back, bad messages are rejected, a client without a valid certificate is refused, and every example configuration parses and refers to files that exist. The TLS and HTTPS tests are compiled out of builds without TLS support.
