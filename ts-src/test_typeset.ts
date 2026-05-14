/// <reference path="./waveforge.d.ts" />

import LONG_TEXT from "./typeset.txt";

const PADDING = 4;
let frameCount = 0;

const root = new waveforge.LayoutNode();
const container = new waveforge.LayoutNode();
const overlay = new waveforge.DrawTextCmd("frame: 0  width: 0", 4, 4, 1, "#ffffff");
const cmds = new waveforge.DrawCmdList();

function buildLayout(): void {
	root.width = 256;
	root.height = 192;
	root.flexDirection = "column";
	root.justifyContent = "center";
	root.alignItems = "center";
	root.backgroundColor = "#1a1a2e";

	container.backgroundColor = "#16213e";
	container.paddingTop = PADDING;
	container.paddingRight = PADDING;
	container.paddingBottom = PADDING;
	container.paddingLeft = PADDING;
	container.contentAlignH = "center";
	container.contentAlignV = "horizon";
	container.content = new waveforge.TextContent(LONG_TEXT, 1, "#e0e0e0");
	container.height = 120;

	root.appendChild(container);

	cmds.push(overlay);
}

waveforge.setupScene({
	size() {
		return [256, 192];
	},

	setup() {
		buildLayout();
		waveforge.log("typeset test ready");
	},

	step() {
		frameCount++;

		const w = 160 + Math.round(Math.sin(frameCount * 0.05) * 60);
		container.width = w;
		container.relayout();

		overlay.text = `frame: ${frameCount}  width: ${w}`;
	},

	render() {
		waveforge.commitLayout(root);
		waveforge.commitDraw(cmds);
	},

	handleEvent(_event: waveforge.SceneEvent) {
		// no-op
	},
});
