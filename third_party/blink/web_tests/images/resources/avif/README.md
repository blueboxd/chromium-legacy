# AVIF Test Files

[TOC]

## Instructions

To add, update or remove a test file, please update the list below.

Please provide full reference and steps to generate the test file so that
any people can regenerate or update the file in the future.

## List of Test Files

### red-(full|limited)-range-(420|422|444)-(8|10|12)bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

Limited:
```
avifenc -r l -d  8 -y 420 -s 0 red.png red-limited-range-420-8bpc.avif
avifenc -r l -d 10 -y 420 -s 0 red.png red-limited-range-420-10bpc.avif
avifenc -r l -d 12 -y 420 -s 0 red.png red-limited-range-420-12bpc.avif
avifenc -r l -d  8 -y 422 -s 0 red.png red-limited-range-422-8bpc.avif
avifenc -r l -d 10 -y 422 -s 0 red.png red-limited-range-422-10bpc.avif
avifenc -r l -d 12 -y 422 -s 0 red.png red-limited-range-422-12bpc.avif
avifenc -r l -d  8 -y 444 -s 0 red.png red-limited-range-444-8bpc.avif
avifenc -r l -d 10 -y 444 -s 0 red.png red-limited-range-444-10bpc.avif
avifenc -r l -d 12 -y 444 -s 0 red.png red-limited-range-444-12bpc.avif
```

Full:
```
avifenc -r f -d  8 -y 420 -s 0 red.png red-full-range-420-8bpc.avif
avifenc -r f -d 10 -y 420 -s 0 red.png red-full-range-420-10bpc.avif
avifenc -r f -d 12 -y 420 -s 0 red.png red-full-range-420-12bpc.avif
avifenc -r f -d  8 -y 422 -s 0 red.png red-full-range-422-8bpc.avif
avifenc -r f -d 10 -y 422 -s 0 red.png red-full-range-422-10bpc.avif
avifenc -r f -d 12 -y 422 -s 0 red.png red-full-range-422-12bpc.avif
avifenc -r f -d  8 -y 444 -s 0 red.png red-full-range-444-8bpc.avif
avifenc -r f -d 10 -y 444 -s 0 red.png red-full-range-444-10bpc.avif
avifenc -r f -d 12 -y 444 -s 0 red.png red-full-range-444-12bpc.avif
```

### TODO(crbug.com/960620): Figure out how the rest of files were generated.
