# Tokens

## Overview

This directory contains strongly-typed wrappers (using
[`util::TokenType<...>`](/base/util/type_safety/token_type.h)) of
[`base::UnguessableToken`](/base/unguessable_token.h)
for tokens that are commonly passed between browsers and renderers. The strong
typing is to prevent type confusion as these tokens are passed around. To support
strong typing through the entire stack (including IPC) these tokens additionally
include `content/` and `blink/` specific typemaps, as well as Mojo struct definitions.

## Adding a new token

Suppose you want to add a new token type, `FooToken`. You would do the following:

 - Create a new `foo_token.h` header in [`/third_party/blink/public/common/tokens`](/third_party/blink/public/common/tokens)
   and define the token using [`util::TokenType<...>`](/base/util/type_safety/token_type.h).
   See [`/third_party/blink/public/common/tokens/worker_tokens.h`](/third_party/blink/public/common/tokens/worker_tokens.h)
   for an example.
 - Create a new `foo_token.mojom` defining a Mojo struct for the token type in
   [`/third_party/blink/public/mojom/tokens`](/third_party/blink/public/mojom/tokens).
   Be sure to follow the convention that the struct contain a single
   `base.mojom.UnguessableToken` member named `value`. See
   [`worker_tokens.mojom`](/third_party/blink/public/mojom/tokens/worker_tokens.mojom) for an example.
 - Create a new `foo_token_mojom_traits.h` header in [`/third_party/blink/public/common/tokens`](/third_party/blink/public/common/tokens)
   that implements `mojo::StructTraits` serialization. Use the templated
   [`TokenMojomTraitsHelper<...>`](/third_party/blink/public/common/token_mojom_traits_helper.h)
   helper class. See [`worker_tokens_mojom_traits.h`](/third_party/blink/public/common/tokens/worker_tokens_mojom_traits.h) for an example.
 - Update [`mojom/tokens/BUILD.gn`](third_party/blink/public/mojom/tokens/BUILD.gn) and add a new
   typemap definition for the token to the `shared_cpp_typemaps` section.
 - If your token needs to be sent via legacy IPC as well, add the appropriate
   definition to [`/content/common/content_param_traits.h`](/content/common/content_param_traits.h).