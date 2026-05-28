#define DUCKDB_EXTENSION_MAIN

#include "duck_slug_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace duckdb {

// ---------------------------------------------------------------------------
// Transliteration table — Latin-1 Supplement (U+00C0–U+00FF) +
//                         Latin Extended-A  (U+0100–U+017F)
// Sorted by code point for binary search.
// ---------------------------------------------------------------------------

struct TranslitEntry {
	char32_t cp;
	const char *replacement;
};

static const std::array<TranslitEntry, 190> transliteration_table = {{
    // --- Latin-1 Supplement ---
    {0x00C0, "a"}, // À
    {0x00C1, "a"}, // Á
    {0x00C2, "a"}, // Â
    {0x00C3, "a"}, // Ã
    {0x00C4, "a"}, // Ä
    {0x00C5, "a"}, // Å
    {0x00C6, "ae"}, // Æ
    {0x00C7, "c"}, // Ç
    {0x00C8, "e"}, // È
    {0x00C9, "e"}, // É
    {0x00CA, "e"}, // Ê
    {0x00CB, "e"}, // Ë
    {0x00CC, "i"}, // Ì
    {0x00CD, "i"}, // Í
    {0x00CE, "i"}, // Î
    {0x00CF, "i"}, // Ï
    {0x00D0, "d"}, // Ð
    {0x00D1, "n"}, // Ñ
    {0x00D2, "o"}, // Ò
    {0x00D3, "o"}, // Ó
    {0x00D4, "o"}, // Ô
    {0x00D5, "o"}, // Õ
    {0x00D6, "o"}, // Ö
    // 0x00D7 × — skip (math symbol)
    {0x00D8, "o"}, // Ø
    {0x00D9, "u"}, // Ù
    {0x00DA, "u"}, // Ú
    {0x00DB, "u"}, // Û
    {0x00DC, "u"}, // Ü
    {0x00DD, "y"}, // Ý
    {0x00DE, "th"}, // Þ
    {0x00DF, "ss"}, // ß
    {0x00E0, "a"}, // à
    {0x00E1, "a"}, // á
    {0x00E2, "a"}, // â
    {0x00E3, "a"}, // ã
    {0x00E4, "a"}, // ä
    {0x00E5, "a"}, // å
    {0x00E6, "ae"}, // æ
    {0x00E7, "c"}, // ç
    {0x00E8, "e"}, // è
    {0x00E9, "e"}, // é
    {0x00EA, "e"}, // ê
    {0x00EB, "e"}, // ë
    {0x00EC, "i"}, // ì
    {0x00ED, "i"}, // í
    {0x00EE, "i"}, // î
    {0x00EF, "i"}, // ï
    {0x00F0, "d"}, // ð
    {0x00F1, "n"}, // ñ
    {0x00F2, "o"}, // ò
    {0x00F3, "o"}, // ó
    {0x00F4, "o"}, // ô
    {0x00F5, "o"}, // õ
    {0x00F6, "o"}, // ö
    // 0x00F7 ÷ — skip (math symbol)
    {0x00F8, "o"}, // ø
    {0x00F9, "u"}, // ù
    {0x00FA, "u"}, // ú
    {0x00FB, "u"}, // û
    {0x00FC, "u"}, // ü
    {0x00FD, "y"}, // ý
    {0x00FE, "th"}, // þ
    {0x00FF, "y"}, // ÿ
    // --- Latin Extended-A ---
    {0x0100, "a"}, // Ā
    {0x0101, "a"}, // ā
    {0x0102, "a"}, // Ă
    {0x0103, "a"}, // ă
    {0x0104, "a"}, // Ą
    {0x0105, "a"}, // ą
    {0x0106, "c"}, // Ć
    {0x0107, "c"}, // ć
    {0x0108, "c"}, // Ĉ
    {0x0109, "c"}, // ĉ
    {0x010A, "c"}, // Ċ
    {0x010B, "c"}, // ċ
    {0x010C, "c"}, // Č
    {0x010D, "c"}, // č
    {0x010E, "d"}, // Ď
    {0x010F, "d"}, // ď
    {0x0110, "d"}, // Đ
    {0x0111, "d"}, // đ
    {0x0112, "e"}, // Ē
    {0x0113, "e"}, // ē
    {0x0114, "e"}, // Ĕ
    {0x0115, "e"}, // ĕ
    {0x0116, "e"}, // Ė
    {0x0117, "e"}, // ė
    {0x0118, "e"}, // Ę
    {0x0119, "e"}, // ę
    {0x011A, "e"}, // Ě
    {0x011B, "e"}, // ě
    {0x011C, "g"}, // Ĝ
    {0x011D, "g"}, // ĝ
    {0x011E, "g"}, // Ğ
    {0x011F, "g"}, // ğ
    {0x0120, "g"}, // Ġ
    {0x0121, "g"}, // ġ
    {0x0122, "g"}, // Ģ
    {0x0123, "g"}, // ģ
    {0x0124, "h"}, // Ĥ
    {0x0125, "h"}, // ĥ
    {0x0126, "h"}, // Ħ
    {0x0127, "h"}, // ħ
    {0x0128, "i"}, // Ĩ
    {0x0129, "i"}, // ĩ
    {0x012A, "i"}, // Ī
    {0x012B, "i"}, // ī
    {0x012C, "i"}, // Ĭ
    {0x012D, "i"}, // ĭ
    {0x012E, "i"}, // Į
    {0x012F, "i"}, // į
    {0x0130, "i"}, // İ
    {0x0131, "i"}, // ı
    {0x0132, "ij"}, // Ĳ
    {0x0133, "ij"}, // ĳ
    {0x0134, "j"}, // Ĵ
    {0x0135, "j"}, // ĵ
    {0x0136, "k"}, // Ķ
    {0x0137, "k"}, // ķ
    {0x0138, "k"}, // ĸ
    {0x0139, "l"}, // Ĺ
    {0x013A, "l"}, // ĺ
    {0x013B, "l"}, // Ļ
    {0x013C, "l"}, // ļ
    {0x013D, "l"}, // Ľ
    {0x013E, "l"}, // ľ
    {0x013F, "l"}, // Ŀ
    {0x0140, "l"}, // ŀ
    {0x0141, "l"}, // Ł
    {0x0142, "l"}, // ł
    {0x0143, "n"}, // Ń
    {0x0144, "n"}, // ń
    {0x0145, "n"}, // Ņ
    {0x0146, "n"}, // ņ
    {0x0147, "n"}, // Ň
    {0x0148, "n"}, // ň
    {0x0149, "n"}, // ŉ
    {0x014A, "n"}, // Ŋ
    {0x014B, "n"}, // ŋ
    {0x014C, "o"}, // Ō
    {0x014D, "o"}, // ō
    {0x014E, "o"}, // Ŏ
    {0x014F, "o"}, // ŏ
    {0x0150, "o"}, // Ő
    {0x0151, "o"}, // ő
    {0x0152, "oe"}, // Œ
    {0x0153, "oe"}, // œ
    {0x0154, "r"}, // Ŕ
    {0x0155, "r"}, // ŕ
    {0x0156, "r"}, // Ŗ
    {0x0157, "r"}, // ŗ
    {0x0158, "r"}, // Ř
    {0x0159, "r"}, // ř
    {0x015A, "s"}, // Ś
    {0x015B, "s"}, // ś
    {0x015C, "s"}, // Ŝ
    {0x015D, "s"}, // ŝ
    {0x015E, "s"}, // Ş
    {0x015F, "s"}, // ş
    {0x0160, "s"}, // Š
    {0x0161, "s"}, // š
    {0x0162, "t"}, // Ţ
    {0x0163, "t"}, // ţ
    {0x0164, "t"}, // Ť
    {0x0165, "t"}, // ť
    {0x0166, "t"}, // Ŧ
    {0x0167, "t"}, // ŧ
    {0x0168, "u"}, // Ũ
    {0x0169, "u"}, // ũ
    {0x016A, "u"}, // Ū
    {0x016B, "u"}, // ū
    {0x016C, "u"}, // Ŭ
    {0x016D, "u"}, // ŭ
    {0x016E, "u"}, // Ů
    {0x016F, "u"}, // ů
    {0x0170, "u"}, // Ű
    {0x0171, "u"}, // ű
    {0x0172, "u"}, // Ų
    {0x0173, "u"}, // ų
    {0x0174, "w"}, // Ŵ
    {0x0175, "w"}, // ŵ
    {0x0176, "y"}, // Ŷ
    {0x0177, "y"}, // ŷ
    {0x0178, "y"}, // Ÿ
    {0x0179, "z"}, // Ź
    {0x017A, "z"}, // ź
    {0x017B, "z"}, // Ż
    {0x017C, "z"}, // ż
    {0x017D, "z"}, // Ž
    {0x017E, "z"}, // ž
    {0x017F, "s"}, // ſ
}};

// ---------------------------------------------------------------------------
// Minimal UTF-8 decoder — advances ptr, returns the code point.
// Returns U+FFFD on invalid/truncated sequences.
// ---------------------------------------------------------------------------

static char32_t DecodeUtf8(const char *&ptr, const char *end) {
	auto b = static_cast<unsigned char>(*ptr++);
	if (b < 0x80) {
		return b;
	}
	if (b < 0xE0) {
		if (ptr >= end) {
			return 0xFFFD;
		}
		return ((b & 0x1F) << 6) | (static_cast<unsigned char>(*ptr++) & 0x3F);
	}
	if (b < 0xF0) {
		if (ptr + 1 >= end) {
			return 0xFFFD;
		}
		char32_t r = static_cast<char32_t>(b & 0x0F) << 12;
		r |= static_cast<char32_t>(static_cast<unsigned char>(*ptr++) & 0x3F) << 6;
		r |= static_cast<unsigned char>(*ptr++) & 0x3F;
		return r;
	}
	if (ptr + 2 >= end) {
		return 0xFFFD;
	}
	char32_t r = static_cast<char32_t>(b & 0x07) << 18;
	r |= static_cast<char32_t>(static_cast<unsigned char>(*ptr++) & 0x3F) << 12;
	r |= static_cast<char32_t>(static_cast<unsigned char>(*ptr++) & 0x3F) << 6;
	r |= static_cast<unsigned char>(*ptr++) & 0x3F;
	return r;
}

// ---------------------------------------------------------------------------
// Core slug logic
// ---------------------------------------------------------------------------

static std::string Slugify(const char *data, size_t len) {
	std::string result;
	result.reserve(len);
	bool pending_sep = false;
	const char *ptr = data;
	const char *end = data + len;

	while (ptr < end) {
		char32_t cp = DecodeUtf8(ptr, end);

		if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) {
			if (pending_sep && !result.empty()) {
				result += '-';
			}
			pending_sep = false;
			result += static_cast<char>(cp);
		} else if (cp >= 'A' && cp <= 'Z') {
			if (pending_sep && !result.empty()) {
				result += '-';
			}
			pending_sep = false;
			result += static_cast<char>(cp + 32);
		} else {
			auto it = std::lower_bound(transliteration_table.begin(), transliteration_table.end(), cp,
			                           [](const TranslitEntry &e, char32_t v) { return e.cp < v; });
			if (it != transliteration_table.end() && it->cp == cp) {
				if (pending_sep && !result.empty()) {
					result += '-';
				}
				pending_sep = false;
				result += it->replacement;
			} else if (!result.empty()) {
				pending_sep = true;
			}
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// DuckDB scalar function wrapper
// ---------------------------------------------------------------------------

inline void SlugifyScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &input = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(input, result, args.size(), [&](string_t s) {
		return StringVector::AddString(result, Slugify(s.GetData(), s.GetSize()));
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	auto fn = ScalarFunction("slugify", {LogicalType::VARCHAR}, LogicalType::VARCHAR, SlugifyScalarFun);
	loader.RegisterFunction(fn);
}

void DuckSlugExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}

std::string DuckSlugExtension::Name() {
	return "duck_slug";
}

std::string DuckSlugExtension::Version() const {
#ifdef EXT_VERSION_DUCK_SLUG
	return EXT_VERSION_DUCK_SLUG;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(duck_slug, loader) {
	duckdb::LoadInternal(loader);
}
}
