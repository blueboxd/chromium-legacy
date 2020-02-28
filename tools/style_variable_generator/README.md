# style_variable_generator

This is a python tool that generates cross-platform style variables in order to
centralize UI constants.

This script uses third_party/pyjson5 to read input json5 files and then
generates various output formats as needed by clients (e.g CSS Variables,
preview HTML page).

For input format examples, see the \*_test.json5 files which contain up to date
illustrations of each feature, as well as expected outputs in the corresponding
\*_test_expected.\* files.

Run `python style_variable_generator.py -h` for usage details.
