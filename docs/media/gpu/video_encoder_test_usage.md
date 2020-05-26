# Video Encoder tests
The video encoder tests are a set of tests that validate various video encoding
scenarios. They run directly on top of the video encoder implementation, and
don't require the full Chrome browser stack. They are built on top of the
[GoogleTest](https://github.com/google/googletest/blob/master/README.md)
framework.

__Note:__ Currently these tests are under development and functionality is still
limited.

[TOC]

## Running from Tast
Running these tests from Tast is not supported yet.

## Running manually
To run the video encoder tests manually the _video_encode_accelerator_tests_
target needs to be built and deployed to the device being tested. Running
the video encoder tests can be done by executing:

    ./video_encode_accelerator_tests [<video path>] [<video metadata path>]

e.g.: `./video_encode_accelerator_tests bear_320x192_40frames.yuv.webm`

__Test videos:__ Various test videos are present in the
[_media/test/data_](https://cs.chromium.org/chromium/src/media/test/data/)
folder in Chromium's source tree (e.g.
[_bear_320x192_40frames.yuv.webm_](https://cs.chromium.org/chromium/src/media/test/data/bear_320x192_40frames.yuv.webm)).
These videos are stored in compressed format and extracted at the start of each
test run, as storing uncompressed videos requires a lot of disk space. Currently
only VP9 or uncompressed videos are supported as test input. If no video is
specified _bear_320x192_40frames.yuv.webm_ will be used.

__Video Metadata:__ These videos also have an accompanying metadata _.json_ file
that needs to be deployed alongside the test video. The video metadata file is a
simple json file that contains info about the video such as its pixel format,
dimensions, framerate and number of frames. These can also be found in the
_media/test/data_ folder (e.g.
[_bear_320x192_40frames.yuv.webm.json_](https://cs.chromium.org/chromium/src/media/test/data/bear_320x192_40frames.yuv.webm.json)).
If no metadata file is specified _\<video path\>.json_ will be used.

## Command line options
Multiple command line arguments can be given to the command:

     -v                  enable verbose mode, e.g. -v=2.
    --vmodule            enable verbose mode for the specified module,
                         e.g. --vmodule=*media/gpu*=2.

    --gtest_help         display the gtest help and exit.
    --help               display this help and exit.

## Source code
See the video encoder tests [source code](https://cs.chromium.org/chromium/src/media/gpu/video_encode_accelerator_tests.cc).

