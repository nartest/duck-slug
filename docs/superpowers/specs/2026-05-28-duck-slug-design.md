# duck_slug Extension — Design Spec

Date: 2026-05-28

## Summary

A DuckDB extension that exposes a single scalar SQL function `slugify(VARCHAR) → VARCHAR`. It transforms a human-readable string into a URL-safe slug: accented Latin characters are transliterated, everything is lowercased, and non-alphanumeric characters are replaced by `-` (with consecutive separators collapsed and leading/trailing dashes trimmed).

## Scope

- Latin Extended-A/B accent removal (French, Spanish, German, Portuguese, etc.)
- Full Unicode or non-Latin script transliteration is out of scope

## SQL Interface

```sql
SELECT slugify('Héllo Wörld!');  -- → 'hello-world'
SELECT slugify('C++ rocks');     -- → 'c-rocks'
SELECT slugify('  --foo-- ');    -- → 'foo'
SELECT slugify(NULL);            -- → NULL
SELECT slugify('');              -- → ''
```

## Architecture

### Files changed / created

| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | Edit | Rename target from `waddle` to `duck_slug`, remove OpenSSL |
| `extension_config.cmake` | Edit | Update `duckdb_extension_load` entry |
| `vcpkg.json` | Edit | Remove `openssl` dependency |
| `src/duck_slug_extension.cpp` | Create | Extension entry point + `slugify` implementation |
| `src/include/duck_slug_extension.hpp` | Create | Extension class declaration |
| `src/waddle_extension.cpp` | Delete | Replaced by duck_slug_extension.cpp |
| `src/include/waddle_extension.hpp` | Delete | Replaced by duck_slug_extension.hpp |
| `test/sql/duck_slug.test` | Create | SQLLogicTest test suite |
| `test/sql/waddle.test` | Delete | Replaced |

### Algorithm (inside `SlugifyScalarFun`)

1. Decode input UTF-8 byte-by-byte into `char32_t` code points
2. For each code point:
   - If ASCII alphanumeric → append `tolower(c)`
   - If found in `transliteration_table` → append the replacement string (may be 1–2 chars, e.g. `ß → "ss"`)
   - Otherwise → mark as separator
3. Separators are collapsed: consecutive separator slots emit a single `-`
4. Trim leading and trailing `-` from the result

### Transliteration table

A `std::array` of `{char32_t, const char*}` pairs, sorted by code point, covering Latin Extended-A (`U+0100–U+017F`) and Latin Extended-B (`U+0180–U+024F`) plus the Latin-1 Supplement accented block (`U+00C0–U+00FF`). Lookup via `std::lower_bound` — O(log n).

Special multi-char mappings: `ß → ss`, `æ → ae`, `œ → oe`, `Þ → th`, `ð → d`.

## NULL and empty string handling

`UnaryExecutor::Execute` in DuckDB propagates NULLs automatically — no explicit check needed. Empty string input produces empty string output (no separators to emit).

## Tests (`test/sql/duck_slug.test`)

| Input | Expected output |
|-------|----------------|
| `'Héllo Wörld!'` | `'hello-world'` |
| `'Ñoño'` | `'nono'` |
| `'straße'` | `'strasse'` |
| `'Hello World 2024'` | `'hello-world-2024'` |
| `'  --foo-- '` | `'foo'` |
| `'C++ rocks'` | `'c-rocks'` |
| `''` | `''` |
| `NULL` | `NULL` |

## Out of scope

- Multi-language transliteration (Cyrillic, CJK, Arabic)
- Custom separator character
- `slugify_strict` variant (fail on untranslatable characters)
