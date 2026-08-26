#ifndef WFORGE_SCENE_H
#define WFORGE_SCENE_H

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>
#include <memory>
#include <proxy/v4/proxy.h>
#include <proxy/v4/proxy_macros.h>
#include <string>
#include <string_view>

namespace wf {

namespace _dispatch {

PRO_DEF_MEM_DISPATCH(MemSceneSize, size);
PRO_DEF_MEM_DISPATCH(MemSceneSetup, setup);
PRO_DEF_MEM_DISPATCH(MemSceneHandleEvent, handleEvent);
PRO_DEF_MEM_DISPATCH(MemSceneStep, step);
PRO_DEF_MEM_DISPATCH(MemSceneRender, render);

}; // namespace _dispatch

class SceneManager;

/* clang-format off */
struct SceneFacade : pro::facade_builder
	::add_convention<_dispatch::MemSceneSize, std::array<int, 2>() const>
	::add_convention<_dispatch::MemSceneSetup, void(SceneManager &)>
	::add_convention<_dispatch::MemSceneHandleEvent, void(SceneManager &, sf::Event &)>
	::add_convention<_dispatch::MemSceneStep, void(SceneManager &)>
	::add_convention<_dispatch::MemSceneRender, void(const SceneManager &, sf::RenderTarget &, int) const>
	::build {};
/* clang-format on */

using Scene = pro::proxy<SceneFacade>;

int automaticScale(int width, int height, int scale_configured = 0);

class SceneManager {
public:
	SceneManager(Scene initial_scene, int scale = 0);
	~SceneManager();

	void changeScene(Scene new_scene);
	void handleEvent(sf::Event &evt);
	void tick();
	void setWindowTitle(std::string title);
	void close() noexcept;

	sf::Vector2i mousePosition() const;
	int scale() const {
		return _scale;
	}

	sf::RenderWindow window;

private:
	void _applyPendingScene();

	Scene _current_scene;
	Scene _pending_scene;
	bool _scene_changed;
	int _config_scale;
	int _scale;
};

class ScriptedScene {
public:
	ScriptedScene(
		const std::string &script_id, std::string_view route_data = ""
	);
	~ScriptedScene();

	std::array<int, 2> size() const;
	void setup(SceneManager &mgr);
	void handleEvent(SceneManager &mgr, const sf::Event &evt);
	void step(SceneManager &mgr);
	void render(
		const SceneManager &mgr, sf::RenderTarget &target, int scale
	) const;

	struct Impl;

private:
	std::unique_ptr<Impl> _impl;
};

} // namespace wf

#endif // WFORGE_SCENE_H
