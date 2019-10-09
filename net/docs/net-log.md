# NetLog

This document describes the design and use of logging through NetLog.

[TOC]

## Adding new NetLogging code

Adding information to the NetLog helps debugging. However, logging also requires
careful review as it can impact performance, privacy, and security.

Please add a [net/log/OWNERS](../log/OWNERS) reviewer when adding new NetLog
parameters, or adding information to existing ones.

The high level objectives when adding net logging code are:

* No performance cost when capturing is off.
* Logs captured using [`kDefault`](../log/net_log_capture_mode.h) are safe to
  upload and share publicly.
* Capturing using [`kDefault`](../log/net_log_capture_mode.h) has a low
  performance impact.
* Logs captured using [`kDefault`](../log/net_log_capture_mode.h) are small
  enough to upload to bug reports.
* Events that may emit sensitive information have accompanying unit-tests.
* The event and its possible parameters are documented in
  [net_log_event_type_list.h](../log/net_log_event_type_list.h)

To avoid doing work when logging is off, logging code should generally be
conditional on `NetLog::IsCapturing()`. Note that when specifying parameters
via a lambda, the lambda is already conditional on `IsCapturing()`.

### Binary data and strings

NetLog parameters are specified as a JSON serializable `base::Value`. This has
some subtle implications:

* Do not use `base::Value::Type::STRING` with non-UTF-8 data.
* Do not use `base::Value::Type::BINARY` (the JSON serializer can't handle it)

Instead:

* If the string is likely ASCII or UTF-8, use `NetLogStringValue()`.
* If the string is arbitrary data, use `NetLogBinaryValue()`.
* If the string is guaranteed to be valid UTF-8, you can use
  `base::Value::Type::STRING`

Also consider the maximum size of any string parameters:

* If the string could be large, truncate or omit it when using the default
  capture mode. Large strings should be relegated to the `kEverything`
  capture mode.

### 64-bit integers

NetLog parameters are specified as a JSON serializable `base::Value` which does
not support 64-bit integers.

Be careful when using `base::Value::SetIntKey()` as it will truncate 64-bit
values to 32-bits.

Instead use `NetLogNumberValue()`.

### Backwards compatibility

There is no backwards compatibility requirement for NetLog events and their
parameters, so you are free to change their structure/value as needed.

That said, changing core events may have consequences for external consumers of
NetLogs, which rely on the structure and parameters to events for pretty
printing and log analysis.

The [NetLog
viewer](https://chromium.googlesource.com/catapult/+/master/netlog_viewer/) for
instance pretty prints certain parameters based on their names, and the event
name that added them.
