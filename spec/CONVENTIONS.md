# C++ Conventions (C++23 mode)

> **Safety first, then readability, then performance.** Prefer functional style and value semantics. All names are **snake_case**.

## 0) Toolchain & Dialect
- You may use all C++23 features 

## 1) Naming, Layout & Structure
- All names use **snake_case**. Namespaces are lower_snake_case.
- One logical public entity per header; keep headers minimal; prefer forward declares.
- Limit lines to ~100 cols; 2‑space indent; trailing commas in multi‑line init‑lists.
- Use #pragma once at the top of files instead of header guards

## 2) Functional‑first design
- Prefer pure free functions with explicit inputs/outputs.
- Prefer value semantics and immutability; use `const`/`constexpr` aggressively.
- Use `[[nodiscard]]` where ignoring a return would be a bug.

## 3) Error handling
- Use `std::expected<T, E>` for recoverable errors; `std::optional<T>` for "maybe" values.
- Reserve exceptions for truly exceptional, non‑local failures. Document them.

## 4) Value categories, moves & forwarding
- Use `std::move(x)` **only** when consuming a **named** object in the current scope.
- Use `std::forward<T>(x)` **only** in functions taking `T&&` where `T` is **deduced** (a forwarding reference).
- Prefer plain `return obj;` and let copy elision/NRVO do the work; never `return std::move(obj);` for local `obj`.

## 5) Modern features (when supported by our Clang)
- Your goal is "no raw loops." Use ranges and algorithms whenever they make sense. 
- `[[no_unique_address]]` for EBO/layout wins (only when measured).
- Template QoL: variadic friends, pack indexing, structured‑binding in conditions, etc. (feature‑test gate).
- Use concepts, constexpr, consteval, static if, static assert, etc. when valuable
- Prefer std::function to raw function pointers.

## 6) API surface & readability
- Use `auto f(args) -> return_type` form; make ownership & mutability explicit.
- Pass cheap scalars by value; large read‑only by `const&`; sink parameters by forwarding refs + perfect forwarding.
- Return values (not output params). Use tuples or small records for multi‑values.

## 7) Memory & lifetime
- No raw owning pointers. Prefer RAII (`std::unique_ptr`, `std::shared_ptr` when required).
- Prefer `std::span`/views for non‑owning access. No naked `new`/`delete` in user code.

## 8) Concurrency
- Prefer `std::jthread` + `std::stop_token`. Avoid data races by defaulting to immutability.

## 9) Diagnostics & build flags
- Build warning‑free with at least:
  ```
  -std=c++23 -Wall -Werror
  ```
- Use sanitizers (`-fsanitize=address,undefined`) in CI.

## 10) Performance (after safety & clarity)
- Prefer clear, idiomatic code; then measure.
- Use `reserve`, `shrink_to_fit`, string_view/span, and rely on copy elision for return‑by‑value.

## 11) Modularity
- Prefer to break code up into modules with clearly defined boundaries.
- Don't repeat yourself. Before writing something, always see if we already have written that.
- Turn commonly used code into libraries.

## 12) Examples

### Forwarding factory
```cpp
template <class T>
auto make_resource(T&& t) {
  return std::make_unique<std::decay_t<T>>(std::forward<T>(t));
}
```

### Return by value (let NRVO/prvalue elision work)
```cpp
auto compute_stats(std::string_view path) -> file_stats {
  file_stats s{/*...*/};
  return s; // not std::move(s)
}
```

### Code Comments

Write comments to explain **why the code exists or why it is implemented in a non-obvious way**, not to document the history of the conversation.

* Comments should describe the current code and its rationale.
* Do not mention previous designs, previous implementations, earlier attempts, discussions, prompts, or decisions made during the session.
* Do not write comments such as "We originally..." or "Previously we..." unless the historical detail is essential to understanding the current code.
* Do not use comments to preserve a narrative of how the code evolved.
* Prefer a short explanation of the invariant, constraint, tradeoff, or reason for the implementation.
* If the code is self-explanatory, do not add a comment merely to describe what it does.

**Good:**

```cpp
// Keep this allocation outside the loop because the buffer is reused for
// every frame.
```

**Bad:**

```cpp
// We originally allocated this inside the loop, but during an earlier
// discussion we decided to move it here because we discovered that...
```

The code should explain itself where possible. Comments should add information that cannot be readily inferred from the code.
