# Shared library compression
## Description
This directory contains the shared library compression tool that allows to
reduce the shared library size by compressing non-performance critical code.
The decompression is done on demand by a watcher thread that is set in a
library constructor. This constructor (being a single .c file) should be
included in the library build to be applied to it.

Additional information may be found in the tracking bug:
https://crbug.com/998082

The tool consists of 2 parts:
### Compression script
This script does the compression part. It removes the specified range from
the file and adds compressed version of it instead. It then sets library
constructor's symbols to point at the cutted range and to the compressed
version.
### Library constructor
Located at `constructor/` path and should be build together with the target
library.

It decompresses data from compressed section, provided by compression script
and populates the target range.

## Usage
Firstly, the library needs to be build with the tool's constructor. To do this
add the following to your build:

    constructor/library_constructor.c

After the library build is complete, the compression script must be applied to
it in the following way:

    ./compress_section.py -i lib.so -o patched_lib.so -l <begin> -r <end>

Where `<begin>` and `<end>` are the file offsets of the part that you want to
compress.

It is important to note that after running the script some of the common ELF
tooling utilities, for example objcopy, may stop working because of the
unusual (but legal) changes made to the library.

## Testing
To run tests:

    test/run_tests.py

