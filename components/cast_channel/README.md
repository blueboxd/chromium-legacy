# How to Run a Fuzz Test

Create an appropriate build config:

```shell
% tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Linux ASan' out/libfuzzer
% gn gen out/libfuzzer
```

Build the fuzz target:

```shell
% ninja -C out/libfuzzer $TEST_NAME
```

Create an empty corpus directory:

```shell
% mkdir ${TEST_NAME}_corpus
```

Run the fuzz target, turning off detection of ODR violations that occur in
component builds:

```shell
% export ASAN_OPTIONS=detect_odr_violation=0
% ./out/libfuzzer/$TEST_NAME ${TEST_NAME}_corpus
```

For more details, refer to https://chromium.googlesource.com/chromium/src/testing/libfuzzer/+/refs/heads/master/getting_started.md
