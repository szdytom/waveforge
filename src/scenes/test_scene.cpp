#include "wforge/assets.h"
#include "wforge/scene.h"
#include <format>
#include <proxy/v4/proxy.h>

namespace wf::scene {

namespace {

constexpr int WIDTH = 256;
constexpr int HEIGHT = 192;

} // namespace

TestScene::TestScene()
	: font(AssetsManager::instance().getAsset<PixelFont>("font"))
	, _duck_texture(
		  AssetsManager::instance().getAsset<sf::Texture>("duck/texture")
	  ) {}

std::array<int, 2> TestScene::size() const {
	return {WIDTH, HEIGHT};
}

void TestScene::setup(SceneManager &mgr) {
	mgr.setWindowTitle("Native Test Scene");
}

void TestScene::handleEvent(SceneManager &mgr, sf::Event &evt) {
	if (auto kb = evt.getIf<sf::Event::KeyPressed>()) {
		if (kb->code == sf::Keyboard::Key::Escape) {
			mgr.changeScene(pro::make_proxy<SceneFacade, MainMenu>());
		}
	}
}

void TestScene::step(SceneManager &mgr) {
	_frame_count++;
	_x += _dx;
	_y += _dy;
	if (_x <= 0 || _x >= WIDTH - static_cast<int>(_duck_texture.getSize().x)) {
		_dx = -_dx;
	}
	if (_y <= 0 || _y >= HEIGHT - static_cast<int>(_duck_texture.getSize().y)) {
		_dy = -_dy;
	}
}

void TestScene::render(
	const SceneManager &mgr, sf::RenderTarget &target, int scale
) const {
	sf::RectangleShape top_bar(sf::Vector2f(WIDTH * scale, 20 * scale));
	top_bar.setPosition(sf::Vector2f(0, 0));
	top_bar.setFillColor(sf::Color(50, 50, 80));
	target.draw(top_bar);

	font.renderText(
		target, "Native Test Scene", sf::Color(200, 200, 50), 10, 3, scale
	);
	font.renderText(
		target, "Press ESC for menu", sf::Color(150, 150, 150), 10, 24, scale
	);
	font.renderText(
		target, std::format("Frame: {}", _frame_count),
		sf::Color(100, 100, 100), 10, HEIGHT - 16, scale
	);

	sf::Sprite duck_sprite(_duck_texture);
	duck_sprite.setPosition(sf::Vector2f(_x * scale, _y * scale));
	duck_sprite.setScale(sf::Vector2f(scale, scale));
	target.draw(duck_sprite);
}

} // namespace wf::scene
