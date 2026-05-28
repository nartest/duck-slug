#define DUCKDB_EXTENSION_MAIN

#include "duck_slug_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
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
