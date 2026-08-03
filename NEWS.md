# LWS for AWS Lambda Release Notes


## Release 0.9.2 beta

- Add DevContainer.
- Add security posture.
- Fix build compatibility with GCC 15.
- Fix table use-after-free and duplicate-key allocation failure.
- Harden table options and hashing.
- Fix zero-capture LWS_MATCH expressions allowing nonmatching request paths.
- Fix HTTP status of generated error responses.
- Fix lws.parseargs handling of additional equals signs and incomplete percent escapes.
- Harden query argument parsing with a configurable field limit defaulting to 255.
- Clear request environments from cached Lua chunks after execution.
- Harden raw JSON array access for invalid indices.
- Optimize raw JSON array iteration and document non-flat array performance.
- Fix conversion of large unsigned raw JSON integers.
- Harden request cleanup ordering.
- Remove broken request body `*a` read hook.
- Fix Set-Cookie serialization with Expires attributes.
- Harden streaming response header lifetime.
- Document unescaped text log output.
- Fix empty response body serialization.
- Harden handling of (invalid) empty Runtime API invocation bodies.
- Harden Runtime API transfer size checks.
- Harden validation of the Lua state request limit.
- Harden empty request metadata handling.
- Document post-request access to request-bound values as undefined.
- Add proper SIGTERM handling with an internal Lambda extension.
- Harden libcurl cleanup after multi errors.
- Reuse the streaming response buffer after flushing.
- Add compatibility with Lua 5.5.
- Improve the example AWS deployment scripts.
- Enhance the default examples with init and pre chunks.


## Release 0.9.1 beta (2025-09-04)

- Add raw headers and body.
- Remove `_DEADLINE_MS` environment variable (now accessible in raw headers).


## Release 0.9.0 beta (2025-08-28)

- Initial public beta release.
