#include "wforge/router.h"
#include <format>
#include <stdexcept>

namespace wf {

namespace {

constexpr std::string_view SCRIPT_PREFIX = "scripts/";

bool isScriptedRoute(std::string_view id) noexcept {
	return id.starts_with(SCRIPT_PREFIX);
}

} // namespace

SceneRouter &SceneRouter::instance() {
	static SceneRouter router;
	return router;
}

void SceneRouter::registerRoute(std::string_view id, Factory factory) {
	_routes[std::string(id)] = std::move(factory);
}

Scene SceneRouter::create(std::string_view id, std::string_view data) {
	// Explicitly registered route takes priority
	auto it = _routes.find(std::string(id));
	if (it != _routes.end()) {
		return it->second(data);
	}

	// Auto-discover scripted scenes from manifest
	if (isScriptedRoute(id)) {
		return pro::make_proxy<SceneFacade, ScriptedScene>(
			std::string(id), data
		);
	}

	throw std::runtime_error(
		std::format("SceneRouter: unknown route '{}'", id)
	);
}

bool SceneRouter::hasRoute(std::string_view id) const {
	if (_routes.contains(std::string(id))) {
		return true;
	}
	return isScriptedRoute(id);
}

} // namespace wf
