#ifndef WFORGE_JS_SCENE_H
#define WFORGE_JS_SCENE_H

#include "wforge/scene.h"
#include <memory>
#include <string>

namespace wf {

class JSScene {
public:
	JSScene(const std::string &scene_id);
	JSScene(JSScene &&) noexcept;
	JSScene &operator=(JSScene &&) noexcept;
	~JSScene();

	JSScene(const JSScene &) = delete;
	JSScene &operator=(const JSScene &) = delete;

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

Scene createSceneFromId(const std::string &scene_id);

} // namespace wf

#endif // WFORGE_JS_SCENE_H
