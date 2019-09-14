# `Tab` lifecycle

`Tab` is a virtual concept that refers to the` WebState` UI. A `WebState` is created then it's 
passed to `AttachTabHelpers` which creates all the other tab helpers. and attaches them to
the `WebState`.

````cpp
web::WebState::CreateParams params{...};
std::unique_ptr<web::WebState> web_state =  web::WebState::Create(params);
AttachTabHelper(web_state.get());
````

When a `WebState` is added to a `TabModel`'s `WebStateList`,
`BrowserWebStateListDelegate` will invoke `AttachTabHelpers` if necessary.

```cpp
TabModel* tab_model = ...;
std::unique_ptr<web::WebState> web_state =  ...;
[tab_model webStateList]->InsertWebState(0, std::move(web_state));
```

All Tab helpers are `WebStateUserData` thus they are destroyed after the
`WebState` destructor completes.
