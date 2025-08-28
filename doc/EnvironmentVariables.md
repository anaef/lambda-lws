# LWS Environment Variables

This document describes the environment variables supported by LWS.


## Configuration Variables

The following variables can be set in the environment of the custom runtime at build time of the
Docker image or when defining an AWS Lambda function based on the image.


### LWS_MATCH *regex*

Sets the match regular expression for HTTP request paths. The resulting capture groups can be
referenced in the `LWS_MAIN` and `LWS_PATH_INFO` variables (see below).

Example value: `^/v1/(\w+)(/?.*)$`

> [!IMPORTANT]
> Depending on where you set the `LWS_MATCH` variable, you may need to escape certain characters,
> in particular backslashes (`\`), to ensure that the intended value is set.

> [!CAUTION]
> If a `LWS_MATCH` capture group is referenced in the `LWS_MAIN` variable, ensure it cannot contain
> punctuation characters that could form relative path segments (e.g., `..`); otherwise, attackers
> might traverse directories and access unintended files.


### LWS_MAIN *main*

Sets the filename of the main Lua chunk. The expression can reference capture groups from the
`LWS_MATCH` variable (see above) through `$0`–`$9` where `$0` is the entire match, `$1` is the
first capture group, and so on. After substitution, the value must be a valid filepath relative to
the task root of the AWS Lambda execution environment. The custom runtime forms the full filename
of the main Lua chunk as `{LAMBDA_TASK_ROOT}/{main}`, e.g., `/var/task/services/echo.lua` if
`LAMBDA_TASK_ROOT` is `/var/task` and the substituted `LWS_MAIN` value is `services/echo.lua`. For
more information, please see the [request processing](RequestProcessing.md) documentation.

Example value: `services/$1.lua`


### LWS_PATH_INFO *path_info*

Sets the path info for requests. The expression can reference capture groups from the `LWS_MATCH`
variable (see above) through `$0`–`$9` where `$0` is the entire match, `$1` is the first capture
group, and so on.

Example value: `$2`


### LWS_INIT *init*

Sets the filepath of an init Lua chunk relative to the task root. This chunk initializes the Lua
state. Please see the [request processing](RequestProcessing.md) documentation for more
information.

Example value: `handler/init.lua`


### LWS_PRE *pre*

Sets the filepath of a pre Lua chunk relative to the task root. This chunk is run before the main
chunk. Please see the [request processing](RequestProcessing.md) documentation for more
information.

Example value: `handler/pre.lua`


### LWS_POST *post*

Sets the filepath of a post Lua chunk relative to the task root. This chunk is run after the main
chunk. Please see the [request processing](RequestProcessing.md) documentation for more
information.

Example value: `handler/post.lua`


### LWS_RAW *raw*

Controls the raw processing mode. In this mode, the custom runtime skips the HTTP semantics
processing of AWS Lambda function URLs and provides the raw request body to the Lua service. This
is suitable for implementing non-HTTP services, such as those driven by AWS Lambda event source
mappings. The *raw* value can take the values `on` and `off`. The default value for *raw* is `off`.
Please see the [request processing](RequestProcessing.md) documentation for more information.


### LWS_GC *gc*

Sets the memory threshold of the Lua state that triggers an explicit garbage collection cycle. If
the memory allocated by the Lua state exceeds *gc* bytes when a request is finalized, an explicit,
full garbage collection cycle is performed. A value of `0`, the default, turns off this logic.
Setting the value to `1` performs a full garbage collection cycle after each request. You can use
the `k` and `m` suffixes with *gc* to set kilobytes or megabytes, respectively.

> [!NOTE]
> The term *memory* in the context of LWS and Lua states generally refers to the memory allocated
> by the Lua state per se, i.e., through its memory allocator. This memory does *not* include
> memory allocated outside of the Lua state, such as in Lua C libraries, the custom runtime, or the
> operating system.


### LWS_REQ_MAX *max_requests*

Sets the maximum number of requests in the lifecycle of the Lua state. If the Lua state has
serviced the specified number of requests, it is closed. A value of `0`, the default, turns off
this logic. Please see the [request processing](RequestProcessing.md) documentation for more
information.


### LWS_DIAGNOSTIC *diagnostic*

Controls the inclusion of diagnostic information in HTTP error responses sent when a Lua error is
generated. The *diagnostic* value can take the values `on` or `off`. The default value for
*diagnostic* is `off`. Diagnostic information includes the error message, file names, line
numbers, function identifiers, and a stack traceback.

> [!NOTE]
> Diagnostic information is logged regardless of the setting of this variable.

> [!CAUTION]
> Diagnostic information can be helpful during development. Enabling diagnostic information in HTTP
> error responses on non-development systems is however not recommended, as such information can be
> exploited by attackers.


### LWS_LOG_LEVEL *log_level*

Sets the log level of the custom runtime. The *log_level* value can take the values `EMERG`,
`ALERT`, `CRIT`, `ERR`, `WARN`, `NOTICE`, `INFO`, and `DEBUG`. The default value for *log_level*
is `INFO`. The log levels are ordered from highest to lowest severity, and *log_level* sets the
minimum severity of messages to be logged.

> [!IMPORTANT]
> Most messages logged with a level of `DEBUG` are only emitted if the custom runtime is built with
> debugging support enabled, i.e., `LWS_DEBUG=1`.


### LWS_LOG_TEXT *log_text*

Controls whether log messages are emitted in text format or in JSON format. The *log_text* value
can take the values `on` or `off`. The default value for *log_text* is `off`, i.e., JSON format.


## Information Variables

The following variables are set by LWS when processing a request.


### _X_AMZN_TRACE_ID

Set to the value of the `Lambda-Runtime-Trace-Id` header, if provided in the AWS Lambda request.
The value is a string representing the AWS X-Ray trace context for the request.


### _DEADLINE_MS

Set to the value of the `Lambda-Runtime-Deadline-Ms` header, if provided in the AWS Lambda request.
The value is a string representing the function timeout as a Unix timestamp in milliseconds.
