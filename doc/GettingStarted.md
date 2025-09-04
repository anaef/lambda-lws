# Getting Started with LWS

LWS comes with two pre-configured Docker images with examples:

* One image uses the *default* processing mode and provides HTTP web service examples with buffered
and streamed responses. The default processing mode is useful for implementing HTTP services, such
as those accessed via AWS Lambda function URLs.

* The other image uses the *raw* processing mode and provides a simple event handler example. The
raw processing mode is useful for implementing non-HTTP services, such as those driven by AWS
Lambda event source mappings. 


## Docker Configuration

The `docker-compose.yml` file describes four services:

* `build-env`: This image is used at build-time only to keep the final images small. It is based on
full Amazon Linux.
* `lambda-lws`: This image provides the abstract custom runtime. It is based on the AWS base image
for *provided* runtimes.
* `lambda-lws-examples-default`: This image provides the web service examples. It is based on the
`lambda-lws` image.
* `lambda-lws-examples-raw`: This image provides the event handler example. It is also based on
the `lambda-lws` image.

The Docker setup is reasonably generic and can be adapted for specific requirements. It uses
the following environment variables which can be set in a `.env` file:

| Variable          | Description                      | Example value    |
| ----------------- | -------------------------------- | ---------------- |
| PACKAGES_BUILD    | Packages required at build time  | libcurl-devel    |
| PACKAGES_RUN      | Packages required at runtime     | libcurl          |
| LUA_VERSION       | Lua version                      | 5.4.8            |
| LUA_ABI           | Lua ABI version                  | 5.4              |
| LUAROCKS_VERSION  | LuaRocks version                 | 3.12.2           |
| LUAROCKS          | Lua rocks required at runtime    | lua-tz luaposix  |


### Web Service Examples

The `lambda-lws-examples-default` service defines the following environment variables:

| Variable        | Value             | Description                                                         |
| --------------- | ----------------- | ------------------------------------------------------------------- |
| LWS_MATCH       | ^/(\w+)(/?.*)$    | Regular expression capturing main Lua chunk and optional path info  |
| LWS_MAIN        | services/$1.lua   | Main Lua chunk filename derived from first capture group            |
| LWS_PATH_INFO   | $2                | Extra path info passed from second capture group                    |
| LWS_DIAGNOSTIC  | on                | Enables diagnostic in HTTP error responses                          |

The `LWS_MATCH` and `LWS_MAIN` values map HTTP request paths to files with main Lua chunks that
implement a web service. Specifically, the path `/{name}/{path_info}` is mapped to the main Lua
chunk `services/{name}.lua` using variable `$1`, which corresponds to the first capture group of
the match expression, i.e., `(\w+)`. Optional extra path information following the service name is
provided as path info to the Lua code using the variable `$2`, which corresponds to the second
capture group of the match expression, i.e., `(/?.*)`.

> [!NOTE]
> This mapping concept mirrors the `lws` directive used in LWS for NGINX.

Additionally, the service enables diagnostic responses using the `LWS_DIAGNOSTIC` variable.


### Event Handler Example

The `lambda-lws-examples-raw` service defines the following environment variables:

| Variable        | Description                                | Value             |
| --------------- | ------------------------------------------ | ----------------- |
| LWS_MAIN        | Main Lua chunk filename (fixed)            | events/event.lua  |
| LWS_RAW         | Raw processing mode (set in `Dockerfile`)  | on                |

As the raw processing mode skips HTTP semantics, there is no HTTP request path mapping, and
`LWS_MAIN` is set to a fixed filename, `events/event.lua`, which contains the main Lua chunk.

For more information, please refer to the [environment variables](EnvironmentVariables.md)
documentation.


## Running and Testing the Examples Locally

The `bin` directory contains the scripts `run-default`, `test-default`, `run-raw`, and
`test-raw`. The run scripts build and run the corresponding Docker service locally using the
Runtime Interface Emulator (RIE). The test scripts use `curl` to send example requests to the
running service.

> [!TIP]
> Before using these scripts, ensure that your `.env` file is correctly configured. You can copy
> the provided `.env.defaults` file to create your own `.env` file and adjust the settings as
> needed.

> [!NOTE]
> When testing with the RIE, you will not see the ultimate HTTP response but its wrapping in the
> AWS Lambda function URL response payload format. Moreover, when testing the streaming response
> example, `curl` will report the presence of binary data in the response. This is expected, as
> streaming responses on AWS Lambda require the use of a binary separator between a JSON prelude
> and the response body. Additionally, with the RIE, streaming responses are not delivered in a
> streaming fashion but buffered until the entire response is available.


## Testing with AWS Lambda

The `bin` directory contains an example `deploy-default` script that performs the following steps:

1.  Create an Elastic Container Registry (ECR) repository if it does not already exist.
2.  Perform a Docker login to the ECR repository.
3.  Build and push the `lambda-lws-examples-default` image to the ECR repository.
4.  Get (or create) an execution role.
5.  Create (or update) the AWS Lambda function using the pushed image and the execution role.
6.  Obtain the version of the deployed function.
7.  Create (or update) two aliases, `buffered` and `stream`, pointing to the deployed version.
8.  Create (or update) two function URLs, one for each alias, configured for buffered and streamed
    responses, respectively.
9.  Allow public invoke of the function URLs.
10. Print the function URLs.

> [!IMPORTANT]
> You may want to adjust various settings in the `deploy-default` script, such as the AWS region.
> By default, the script uses the `eu-central-1` region (Frankfurt) and makes the function URLs
> publicly accessible without authentication on the Internet.

After the function is successfully deployed, you can use `curl` to send requests to the function
URLs, for example:

```sh
curl {your_buffered_function_url}/echo/hello?param1=arg1
curl {your_stream_function_url}/stream
```

> [!TIP]
> The script uses aliases to create *two* function URLs for a single deployed version of the
> function. This deployment architecture was chosen because the examples demonstrate both buffered
> and streamed responses, which need to be configured differently (`--invoke-mode RESPONSE_STREAM`
> in case of the streamed function URL.) You can verify this necessity by requesting the `echo`
> service via the streamed function URL, and the `stream` service via the buffered function URL.
> Both will not produce the desired results. Outside of examples, you would often deploy two
> separate functions if you need both buffered and streamed responses. This simplifies the
> deployment architecture, as function URLs can be configured directly for each function without
> the need for aliases, and it may also reduce costs.

Similarly, the `bin` directory contains an example `deploy-raw` script that performs steps 1–5 from
the above list for the `lambda-lws-examples-raw` image. Note that the `deploy-raw` script does not
configure a function URL, as the raw processing mode is typically used with event source mappings.

> [!IMPORTANT]
> You may want to adjust various settings in the `deploy-raw` script, such as the AWS region.
> By default, the script uses the `eu-central-1` region (Frankfurt).

After the function is successfully deployed, you can use the AWS CLI to test the function, for
example:

```sh
aws lambda invoke --region eu-central-1 --function-name lambda-lws-examples-raw --cli-binary-format raw-in-base64-out --payload '{"confirm": true}' response.json
cat response.json
```
