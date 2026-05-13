/// <reference path="./waveforge.d.ts" />

const WIDTH = 256;
const HEIGHT = 192;
let frameCount = 0;
let x = 50, y = 50, dx = 2, dy = 1;

const duck = new waveforge.Texture("duck/texture");
const cmds = new waveforge.DrawCmdList();
const drawDuckCmd = new waveforge.DrawSpriteCmd(duck, x, y);
const bgRect = new waveforge.DrawRectCmd(0, 0, WIDTH, HEIGHT, "#c8dcff");
const redRect = new waveforge.DrawRectCmd(10, 10, 40, 30, "#ff6464");
const fpsText = new waveforge.DrawTextCmd("FRAME: 0", 4, 4, 1, "#000000");

bgRect.color = "#dcf0ff";

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
