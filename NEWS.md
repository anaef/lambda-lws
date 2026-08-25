# LWS for AWS Lambda Release Notes


## Release 0.9.3 beta

- Fix compiler branch hint for calloc memory allocation failure.
- Harden table with random hash seed.


## Release 0.9.2a beta (2026-08-03)

- Add DevContainer.
- Add security posture.
- Add proper SIGTERM handling with an internal Lambda extension.
- Add compatibility with Lua 5.5.
- Clear request environments from cached Lua chunks after execution.
- Optimize raw JSON array iteration and document non-flat array performance.
- Remove broken request body `*a` read hook.
- Document unescaped text log output.
- Document post-request access to request-bound values as undefined.
- Reuse the streaming response buffer after flushing.
- Improve the example AWS deployment scripts.
- Enhance the default examples with init and pre chunks.
- Fix build compatibility with GCC 15.
- Fix table use-after-free and duplicate-key allocation failure.
- Fix zero-capture LWS_MATCH expressions allowing nonmatching request paths.
- Fix HTTP status of generated error responses.
- Fix lws.parseargs handling of additional equals signs and incomplete percent escapes.
- Fix conversion of large unsigned raw JSON integers.
- Fix Set-Cookie serialization with Expires attributes.
- Fix empty response body serialization.
- Harden table options and hashing.
- Harden query argument parsing with a configurable field limit defaulting to 255.
- Harden raw JSON array access for invalid indices.
- Harden request cleanup ordering.
- Harden streaming response header lifetime.
- Harden handling of (invalid) empty Runtime API invocation bodies.
- Harden Runtime API transfer size checks.
- Harden validation of the Lua state request limit.
- Harden empty request metadata handling.
- Harden libcurl cleanup after multi errors.


## Release 0.9.1 beta (2025-09-04)

- Add raw headers and body.
- Remove `_DEADLINE_MS` environment variable (now accessible in raw headers).


## Release 0.9.0 beta (2025-08-28)

- Initial public beta release.
