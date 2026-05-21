#ifndef WFORGE_ROUTER_H
#define WFORGE_ROUTER_H

#include "wforge/scene.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace wf {

class SceneRouter {
public:
	using Factory = std::function<Scene(std::string_view data)>;

	static SceneRouter &instance();

	void registerRoute(std::string_view id, Factory factory);

	Scene create(std::string_view id, std::string_view data = "");

	[[nodiscard]] bool hasRoute(std::string_view id) const;

private:
	SceneRouter() = default;

	std::unordered_map<std::string, Factory> _routes;
};

} // namespace wf

#endif // WFORGE_ROUTER_H
