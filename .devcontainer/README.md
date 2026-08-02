# Lambda LWS Dev Container

The development container provides a Linux environment for building and testing Lambda LWS. It
uses Ubuntu 26.04, GCC, GDB, Valgrind, yyjson, libcurl, and Lua 5.1 through 5.5, with Lua 5.4 used
by default. AWS CLI version 2 is also installed.

The repository is mounted at `/workspaces/lambda-lws`. Development commands are stored in
`/workspaces/lambda-lws/.devcontainer/bin`, which is added to `PATH`. The project-level `bin`
directory is deliberately not added to `PATH`; it contains separate
[example AWS deployment scripts](../doc/GettingStarted.md#testing-with-aws-lambda) that also work
outside of the development container.

The container reuses the host Docker daemon through the forwarded Docker socket. It does not run a
second Docker daemon. Docker images and containers created from the development container are
therefore visible in Docker Desktop and share its image cache.

Docker socket access grants control over the host Docker daemon. Only open trusted repositories in
the development container.


## Configuration

You can copy the provided `.env.defaults` file to create your own `.env` file and adjust the
settings as needed.

Docker Desktop selects the native Linux architecture automatically. Apple Silicon systems use
Linux ARM64, while conventional Windows and Intel systems use Linux AMD64. The local Compose
configuration does not force an architecture.


## Native development

Build the runtime with GCC, debug symbols, and debug logging support:

```bash
lws-build
```

Build and run the codec unit test:

```bash
lws-test
```

Both commands use Lua 5.4 by default. Set `LUA_ABI` for an individual command to use another
installed ABI:

```bash
LUA_ABI=5.3 lws-build
```


## Local Lambda runtime

Build and start the default example image under the AWS Lambda Runtime Interface Emulator in one
terminal:

```bash
lws-run
```

Exercise it from another terminal:

```bash
lws-invoke
lws-stop
```

Additional invocations can be sent before stopping the runtime. Runtime output remains visible in
the `lws-run` terminal. Finish the run with `lws-stop` in the second terminal.

The optional mode argument selects the raw example. Start it in one terminal:

```bash
lws-run raw
```

Then exercise and stop it from another terminal:

```bash
lws-invoke raw
lws-stop
```

Only one example can listen on [http://localhost:8080](http://localhost:8080) at a time.
`lws-run` replaces any existing Lambda LWS Compose runtime before starting the selected service.
If port 8080 is already in use, set `LWS_PORT` to another host port in `.env`; the container port
remains 8080.


## Valgrind

Valgrind uses `.devcontainer/Dockerfile.valgrind` and
`.devcontainer/docker-compose.valgrind.yml`. The harness first builds the ordinary Amazon Linux
2023 example image from the deployment Dockerfile, then derives a development-only image with an
unoptimized, debug-symbol runtime and Valgrind. The root Dockerfile and Compose file remain the
deployment examples documented in the project guide.

Start the default example under Valgrind in one terminal:

```bash
lws-valgrind
```

Exercise it from another terminal:

```bash
lws-invoke
```

Additional invocations can be sent before stopping the runtime. Finish the run with `lws-stop` in
the second terminal, or press Ctrl-C in the Valgrind terminal. The runtime receives SIGTERM and
performs its normal cleanup before Valgrind completes the report.

Use the `raw` argument for the raw example:

```bash
lws-valgrind raw
lws-invoke raw
```

The report is displayed in the Valgrind terminal and retained at
`/tmp/lambda-lws/valgrind-default.log` or `/tmp/lambda-lws/valgrind-raw.log`.

No suppression file is used. Any future suppression must be limited to a confirmed
dependency-owned allocation and must not suppress Lambda LWS call stacks.
