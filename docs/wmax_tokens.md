# -Wmax-tokens

This is an experiment that uses the compiler to limit the size of certain header
files as a way of tackling #include bloat.

The main idea is to insert "back stops" for header sizes to ensure that header
cleanups don't regress without anyone noticing.

A back stop is implementing using this pragma:

```c++
#pragma clang max_tokens_here <num>
```

It makes the compiler issue a -Wmax-tokens warning (which we treat as an error)
if the number of tokens after preprocessing at that point in the translation
unit exceeds num.

For more background and data, see
[clang -Wmax-tokens: A Backstop for #include Bloat](https://docs.google.com/document/d/1xMkTZMKx9llnMPgso0jrx3ankI4cv60xeZ0y4ksf4wc/preview).


## How to insert a header size back stop

To limit the size of a header foo.h, insert the pragma directly after including
the header into its corresponding .cc file:

```c++
// in foo.cc:
#include "foo.h"
#pragma clang max_tokens_here 123456

#include "blah.h"
```

By inserting the pragma at this point, it captures the token count from
including the header in isolation.

In order to not create unnecessary friction for developers, token limits should
only be imposed on headers with significant impact on build times: headers that
are (or were) large and widely included.


## What to do when hitting a -Wmax-tokens warning

Try using forward declarations or otherwise restructuring your change to
avoid growing the header beyond the limit. If that is infeasible, raise the
limit.
