# HTTP File Server

HTTP/1.1 file server that reads and writes files on disk via `GET` and `PUT`. The server can run single-threaded or with a worker pool and serializes conflicting file access per-URI.

## Build
- Requirements: POSIX environment with `clang`/`gcc`, `make`, and pthreads.
- From the repo root run `make` to produce the `./httpserver` binary. Use `make clean` to remove build artifacts.

## Run
```
./httpserver [-p threads] <port> [data-dir]
```
- `port` must be 1–65535.
- `data-dir` is the directory to serve from; defaults to the current directory. Tests set `TEST_TMPDIR` to override this path automatically.
- `-p threads` enables a thread pool to handle connections concurrently. Each request is still guarded by a per-URI mutex to prevent conflicting reads/writes on the same path.
- Stop with Ctrl+C.

Example:
```
./httpserver -p 4 8080 ./data
```

## Request/Response Behavior
- Supported methods: `GET` and `PUT` only. Others return `501 Not Implemented`.
- Protocol: only `HTTP/1.1` is accepted; other versions return `505 Version Not Supported`.
- URIs: must start with `/` and contain 1–63 characters of `A–Z`, `a–z`, `0–9`, `-`, or `.`. Directory traversal is not allowed; the URI maps directly to a file under `data-dir`.
- Headers:
  - `Content-Length` is required for `PUT`; its value must be a non-negative integer.
  - Optional `Request-ID` header is logged when present.
- `GET /path`  
  - `200 OK` with file bytes in the body when readable.  
  - `404 Not Found` if the file is missing.  
  - `403 Forbidden` if the path is a directory or not readable.
- `PUT /path` with a body sized by `Content-Length`  
  - `201 Created` when the file did not previously exist.  
  - `200 OK` when overwriting an existing, writable file.  
  - `403 Forbidden` when the target exists but is not writable.
- Generic failures return `400 Bad Request` for malformed input or `500 Internal Server Error` for unexpected errors. Error responses include a short text body.

## Logging
Audit logs are written to `stderr` in CSV form:
```
<METHOD>,<URI>,<status>,<Request-ID>
```
`Request-ID` is `0` when the header is absent.

## Tests
- Module statuses are tracked in `tests/test-config.txt` (all modules are currently marked `DONE`).
- Run the full suite with `./tests/run_test.sh` from the repo root. The script compiles objects into `obj/ci`, sets `TEST_TMPDIR` for isolated I/O, and exercises parser, file, dispatcher, logging, server, connection, parallel, and main routines.
