#include "wforge/runtime.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace wf::js {

ModuleRegistry &ModuleRegistry::instance() {
	static ModuleRegistry reg;
	return reg;
}

void ModuleRegistry::loadFromMetafile(
	const std::filesystem::path &metafile_path,
	const std::filesystem::path &assets_root
) {
	auto meta = nlohmann::json::parse(std::ifstream(metafile_path));
	const auto &outputs = meta.at("outputs");

	for (auto it = outputs.begin(); it != outputs.end(); ++it) {
		const std::string &path_str = it.key();
		// Skip metafile itself
		if (path_str.ends_with(".metafile.json")) {
			continue;
		}
		// Skip non-JS outputs
		if (!path_str.ends_with(".js")) {
			continue;
		}

		// Metafile paths are relative to project root: "assets/bundled-js/..."
		// Strip "assets/" prefix → "bundled-js/..."
		// If path is already absolute, use it directly.
		std::string key;
		if (path_str.starts_with("assets/")) {
			key = path_str.substr(7); // len("assets/") = 7
		} else if (path_str.starts_with("./assets/")) {
			key = path_str.substr(9); // len("./assets/") = 9
		} else {
			// Absolute path or other format — try relative from assets root
			auto rel = std::filesystem::path(path_str).lexically_relative(
				assets_root
			);
			key = rel.generic_string();
		}
		auto full_path = assets_root / key;
		std::ifstream file(full_path);
		if (!file.is_open()) {
			std::cerr << "ModuleRegistry: warning - cannot open " << full_path
					  << "\n";
			continue;
		}

		_sources[std::move(key)] = std::string(
			std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>()
		);
	}

	std::cerr << "ModuleRegistry: loaded " << _sources.size()
			  << " JS module(s) from metafile\n";
}

const std::string *ModuleRegistry::find(const std::string &module_name) const {
	auto it = _sources.find(module_name);
	if (it != _sources.end()) {
		return &it->second;
	}
	return nullptr;
}

std::string ModuleRegistry::entryModuleFor(const std::string &scene_id) {
	// "scripts/react_hello" → "bundled-js/react_hello.js"
	auto slash = scene_id.find('/');
	std::string name = (slash != std::string::npos)
		? scene_id.substr(slash + 1)
		: scene_id;
	return "bundled-js/" + name + ".js";
}

} // namespace wf::js

// ── Module loader callbacks (C linkage for QuickJS) ──

extern "C" char *moduleNormalizer(
	JSContext *ctx, const char *module_base_name, const char *module_name,
	void *opaque
) {
	(void)opaque;

	// For bare specifiers (non-relative), pass through
	if (module_name[0] != '.') {
		auto *result = static_cast<char *>(
			js_malloc(ctx, strlen(module_name) + 1)
		);
		if (result) {
			strcpy(result, module_name);
		}
		return result;
	}

	// Resolve relative path against base module name's directory
	std::filesystem::path base(module_base_name);
	auto resolved = base.parent_path() / module_name;
	resolved = resolved.lexically_normal();
	auto resolved_str = resolved.generic_string();

	auto *result = static_cast<char *>(js_malloc(ctx, resolved_str.size() + 1));
	if (result) {
		memcpy(result, resolved_str.data(), resolved_str.size());
		result[resolved_str.size()] = '\0';
	}
	return result;
}

extern "C" JSModuleDef *moduleLoader(
	JSContext *ctx, const char *module_name, void *opaque,
	JSValueConst attributes
) {
	(void)attributes;
	auto *registry = static_cast<wf::js::ModuleRegistry *>(opaque);

	const std::string *source = registry->find(module_name);
	if (!source) {
		JS_ThrowReferenceError(ctx, "Module not found: %s", module_name);
		return nullptr;
	}

	JSValue val = JS_Eval(
		ctx, source->c_str(), source->size(), module_name,
		JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY
	);
	if (JS_IsException(val)) {
		return nullptr;
	}

	if (wf::js::setModuleImportMeta(ctx, val) < 0) {
		JS_FreeValue(ctx, val);
		return nullptr;
	}

	auto *m = static_cast<JSModuleDef *>(JS_VALUE_GET_PTR(val));
	JS_FreeValue(ctx, val); // Module is held alive by the engine
	return m;
}

extern "C" int moduleCheckAttributes(
	JSContext *ctx, void *opaque, JSValueConst attributes
) {
	(void)ctx;
	(void)opaque;
	(void)attributes;
	// Accept all import attributes
	return 0;
}

// ── import.meta initializer (simplified, no quickjs-libc dependency) ──

namespace wf::js {

int setModuleImportMeta(JSContext *ctx, JSValueConst func_val) {
	auto *m = static_cast<JSModuleDef *>(JS_VALUE_GET_PTR(func_val));

	JSAtom name_atom = JS_GetModuleName(ctx, m);
	const char *name = JS_AtomToCString(ctx, name_atom);
	JS_FreeAtom(ctx, name_atom);
	if (!name) {
		return -1;
	}

	// Build "file://<module_name>" URL
	std::string url = "file://";
	if (name[0] != '/') {
		url += '/';
	}
	url += name;

	JSValue meta = JS_GetImportMeta(ctx, m);
	if (!JS_IsObject(meta)) {
		JS_FreeCString(ctx, name);
		return -1;
	}

	JS_DefinePropertyValueStr(
		ctx, meta, "url", JS_NewString(ctx, url.c_str()),
		JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
	);

	JS_FreeValue(ctx, meta);
	JS_FreeCString(ctx, name);
	return 0;
}

} // namespace wf::js
