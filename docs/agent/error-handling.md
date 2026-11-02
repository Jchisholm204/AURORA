# Error Handling

Rules for error paths in AURORA. Apply these to any change that adds a call that
can fail, a new public function, or a new failure branch.

## Check and return

- Check the return value of every fallible call. The only exception is deliberate
  best-effort cleanup, marked with `(void)`.
- Return the component's error type: `e<PREFIX>_OK` on success, an
  `e<PREFIX>_ERR_*` value on failure.
- Constructors (`*_create_instance`, `*_init`) return `NULL` on failure.
- Translate foreign errors (UCS, `errno`, another component) into the component's
  own error type at the boundary. Do not leak raw foreign codes upward.

## Null contracts

- Public entry points validate the arguments they cannot work without, then fail
  cleanly with the component's error type (or `NULL` for constructors).
- Internal interfaces assume required handles and objects are non-null. Do not
  add redundant checks — enforce the guarantee with `assert()` instead.
- Destructors tolerate `NULL`, reset handles they free to `NULL`, and are safe to
  call twice.
- `assert()` is for internal programmer-error invariants only. Never use it to
  validate external input or to handle a recoverable runtime failure.

## Logging

Use the project logging library from `log.h`: `log_trace`, `log_debug`,
`log_info`, `log_warn`, `log_error`, `log_fatal`. Do not use `printf`,
`fprintf`, or `perror` for diagnostics.

Every error return must be logged. Logging in more than one layer is fine; avoid
repeating the same message in a loop.

Follow the existing message style: short, capitalized fragments with no trailing
period, including the error code when one is available.

| Situation | Message |
|---|---|
| Required pointer is `NULL` | `log_error("NULL Parameter");` |
| Allocation failed | `log_error("Bad Alloc??");` |
| Component error code | `log_error("ARM Error: %d", status);` |
| Operation failed | `log_error("Failed to create ACI instance");` |
| Not initialized | `log_fatal("Not Initialized");` |
| Broken invariant | `log_fatal("THREAD COLLISION");` |
| Degraded but continuing | `log_warn("Failed to set SO_REUSEADDR");` |

Severity:

- `log_fatal` — an invariant is broken or the instance cannot continue. It is a
  severity, not a process abort; control still returns to the caller.
- `log_error` — the operation failed.
- `log_warn` — degraded, retryable, or continuing with reduced function.
- `log_info` / `log_debug` / `log_trace` — expected negative outcomes and
  diagnostics.

Never log-and-swallow: if you log an error, still return or propagate it.

## Cleanup

- Every failure path releases what it acquired.
- Use one cleanup label per function, free in reverse order, and set freed
  pointers to `NULL`.
- No `abort()` or `exit()` in library code.

## Checklist

Before finishing a change, confirm:

1. Every fallible call's return value is checked, or explicitly `(void)`.
2. Failure paths return the component's error type; constructors return `NULL`.
3. Public boundaries validate inputs; internal functions assume non-null and use
   `assert()` for that guarantee.
4. Every error return is logged with the project log library and message style.
5. Foreign errors are translated, not leaked.
6. Every failure path cleans up, and freed handles are set to `NULL`.
