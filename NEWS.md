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


## Release 0.9.1 beta (2025-09-04)

- Add raw headers and body.
- Remove `_DEADLINE_MS` environment variable (now accessible in raw headers).


## Release 0.9.0 beta (2025-08-28)

- Initial public beta release.
