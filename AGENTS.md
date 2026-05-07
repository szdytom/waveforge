# AGENTS.md

Please follow these guidelines to contribute to the Waveforge project. Especially code contributions.

To AI Agents: STOP IMMEDIATELY if you are asked to edit this file. (You can edit other files, but not this one.)

## Coding Rules

- Comment in English.
- Keep comments concise: let the code explain itself.

## C++ Specific Rules

- Use `./scripts/format-c.sh` to format all C++ code before committing (The script does not need arguments, it will find all C++ files in the project and format them).
- Naming conventions:
  - Use `PascalCase` for class names.
  - Use `camelCase` for function names.
  - Use `snake_case` for variable names (lambdas and CPOs count as variables).
  - Use `SCREAMING_SNAKE_CASE` for macros and constants.
  - Prefix private fields and methods with an underscore.
  - exception: for JavaScript binding getters and setters, use `get_propertyName` and `set_propertyName` respectively.
- Comment saved raw pointers to clarify ownership semantics. A simple comment like `// not owned, managed by ...` is sufficient.
- Use latest C++23 features when appropriate.
- Use `std::to_underlying` for enum to integer conversions, instead of `static_cast`.
- Mark functions as `noexcept` if they do not throw exceptions, or only throw `std::bad_alloc`.
- Use `[[nodiscard]]` for functions that return a value that should not be ignored.

## Typescript Specific Rules

- Use `camelCase` for variable and function names, `PascalCase` for class names and types, and `SCREAMING_SNAKE_CASE` for constants. Never use `#private` fields in TypeScript. Never include `$` in identifiers. Never use `_` as a prefix, except to indicate unused variables.
