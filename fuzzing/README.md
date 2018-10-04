Fuzzing targets used by [oss-fuzz](https://github.com/google/oss-fuzz/).

## How to reproduce oss-fuzz bugs locally

A somewhat recent version of clang is recommended.

Build with at least the following flags, choosing a sanitizer as needed.

```
$ CC=clang CXX=clang++ meson DIR -Db_sanitize=<address|undefined> -Db_lundef=false
```

Afterwards run the affected target against the provided test case.

```
$ DIR/fuzzing/fuzz_target_name FILE
```

#### FAQs

- What about Memory Sanitizer (MSAN)?

  Correct MSAN instrumentation is [difficult to achieve](https://clang.llvm.org/docs/MemorySanitizer.html#handling-external-code) locally, so false positives are very likely. If need be, [you can still reproduce](https://github.com/google/oss-fuzz/blob/master/docs/reproducing.md#building-using-docker) those bugs with the oss-fuzz provided docker images.

- There are no file/function names in the stack trace.

  `llvm-symbolizer` (usually installed with LLVM) must be in `$PATH`.

- UndefinedBehavior Sanitizer (UBSAN) doesn't provide a stack trace.

  You may need to export environment variable `UBSAN_OPTIONS=print_stacktrace=1` before running the target.
