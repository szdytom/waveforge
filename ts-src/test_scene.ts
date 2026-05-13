/// <reference path="./waveforge.d.ts" />

const WIDTH = 256;
const HEIGHT = 192;
let frameCount = 0;
let x = 50, y = 50, dx = 2, dy = 1;

const duck = new waveforge.Texture("duck/texture");
const cmds = new waveforge.DrawCmdList();
const drawDuckCmd = new waveforge.DrawSpriteCmd(duck, x, y);
const bgRect = new waveforge.DrawRectCmd(0, 0, WIDTH, HEIGHT, new waveforge.Color(200, 220, 255));
const redRect = new waveforge.DrawRectCmd(10, 10, 40, 30, new waveforge.Color(255, 100, 100));
const fpsText = new waveforge.DrawTextCmd("FRAME: 0", 4, 4, 1, new waveforge.Color(0, 0, 0));

bgRect.r = 220;
bgRect.g = 240;
bgRect.b = 255;

cmds.push(bgRect);
cmds.push(redRect);
cmds.push(drawDuckCmd);
cmds.push(fpsText);

waveforge.log("Loaded " + duck.width + "x" + duck.height + " duck texture");
waveforge.log("Test scene loading...");

waveforge.setupScene({
    size() {
        return [WIDTH, HEIGHT];
    },

    setup() {
        waveforge.log("Test scene setup complete");
    },

    step() {
        frameCount++;
        x += dx;
        y += dy;
        if (x <= 0 || x >= WIDTH - duck.width) dx = -dx;
        if (y <= 0 || y >= HEIGHT - duck.height) dy = -dy;
        drawDuckCmd.x = x;
        drawDuckCmd.y = y;
        fpsText.text = "FRAME: " + frameCount;
    },

    handleEvent(event: waveforge.SceneEvent) {
		if (event.type === "mousemove") {
			return; // Too noisy
		}
        waveforge.log("Event: " + event.type + ("code" in event ? " code=" + event.code : ""));
    },

    render() {
        waveforge.commitDraw(cmds);
    },
});
