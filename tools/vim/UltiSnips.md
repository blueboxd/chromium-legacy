# UltiSnips

[UltiSnips](https://vimawesome.com/plugin/ultisnips) is one of snippet systems
for vim.
Below are some UltiSnips snippets that other Chromium developers will hopefully
find useful.

## C++

### Copyright

```UltiSnips
snippet copyright
// Copyright `date +%Y` The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$0
endsnippet
```


### Include guard

This relies on current directory being the Chromium root directory.

```Ultisnips
snippet ifndef "include guard" w
ifndef ${1:`!p snip.rv=str(path).replace('/','_').replace('\\','_').replace('.','_').upper()+'_'`}
#define $1

$0

#endif  // $1
endsnippet
```


### Ad-hoc logging

```Ultisnips
snippet LOG(ERROR)
LOG(ERROR) << __func__$0;
endsnippet

snippet "<<.(.+)\.logme" "Logging expansion" r
<< "; `!p snip.rv = match.group(1).replace('"', '\\"')` = " << `!p snip.rv = match.group(1)`$0
endsnippet

snippet "<<.(.+)\.logint" "Logging expansion" r
<< "; `!p snip.rv = match.group(1).replace('"', '\\"')` = " << static_cast<int>(`!p snip.rv = match.group(1)`)$0
endsnippet

snippet "<<.stack" "Logging expansion2" r
<< "; stack = " << base::debug::StackTrace().ToString()
endsnippet

snippet "<<.jstack" "Logging expansion2" r
<< "; js stack = "
           << [](){
              char* buf = nullptr; size_t size = 0;
              FILE* f = open_memstream(&buf, &size);
              if (!f) return std::string("<mem error>");
              v8::Message::PrintCurrentStackTrace(
                  v8::Isolate::GetCurrent(), f);
              fflush(f); fclose(f);
              std::string result(buf, size);
              free(buf);
              return result; }();
endsnippet
```
