# Layered API resources directory

This directory stores module JavaScript implementation files of
[Layered APIs](https://github.com/drufball/layered-apis),
that are to be bundled with the browser.

## Adding/removing files

When adding/removing files, execute

- `core/script/generate_lapi_grdp.py`
- `git cl format`

to update the following files:

- `core/script/layered_api_resources.h`
- `core/script/resources/layered_api/resources.grdp`

and commit these files together with the changes under resources/layered_api/.

(This is not needed when the content of the files are modified)

## Which files are bundled

All files under sub-directories which have 'index.mjs' will be included
in the grdp and thus bundled in the Chromium binary, except for files
starting with '.', 'README', or 'OWNERS'.

So be careful about binary size increase when you add new files or add more
contents to existing files.
