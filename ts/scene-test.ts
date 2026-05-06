import type { SceneEvent } from "./waveforge";

waveforge.log("Test scene loading...");

const WIDTH = 256;
const HEIGHT = 192;
let frameCount = 0;
let x = 50, y = 50, dx = 2, dy = 1;
const duck = new waveforge.Texture("duck/texture");

waveforge.log(`Loaded ${duck.width}x${duck.height} duck texture`);

waveforge.setupScene({
	size() {
		return [WIDTH, HEIGHT];
	},

	setup() {
		waveforge.log("Test scene setup complete");
	},

	handleEvent(event: SceneEvent) {
		if (event.type === "keyPressed" && event.key === "Escape") {
			waveforge.changeScene("main-menu");
		}
	},

	step() {
		frameCount++;
		x += dx;
		y += dy;
		if (x <= 0 || x >= WIDTH - duck.width) dx = -dx;
		if (y <= 0 || y >= HEIGHT - duck.height) dy = -dy;
	},

	render() {
		const cmds = new waveforge.DrawCmdBuffer();
		cmds.add(new waveforge.DrawRect(0, 0, WIDTH, 20, 50, 50, 80));
		cmds.add(new waveforge.DrawText(10, 3, "JS Test Scene", 1, 200, 200, 50));
		cmds.add(new waveforge.DrawText(10, 24, "Press ESC for menu", 1, 150, 150, 150));
		cmds.add(new waveforge.DrawText(10, HEIGHT - 16, "Frame: " + frameCount, 1, 100, 100, 100));
		cmds.add(new waveforge.DrawSprite(x, y, duck));
		waveforge.commitDrawCmds(cmds);
	},
});
