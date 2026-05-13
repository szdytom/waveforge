/// <reference path="./waveforge.d.ts" />

const W = 256;
const H = 192;

let frameCount = 0;
let layoutRoot: waveforge.LayoutNode;

const fpsText = new waveforge.DrawTextCmd("FRAME: 0", 4, 160, 1, "#ffffff");

const duck = new waveforge.Texture("duck/texture");
const cmds = new waveforge.DrawCmdList();
cmds.push(fpsText);

function createNode(
	props: Partial<waveforge.LayoutNode>,
): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	Object.assign(node, props);
	return node;
}

function buildLayout(): waveforge.LayoutNode {
	const root = createNode({ width: W, height: H, flexDirection: "column" });

	// ── 1. Header ──
	const header = createNode({
		height: 16,
		backgroundColor: "#282c34",
		flexDirection: "row",
		alignItems: "center",
		justifyContent: "center",
	});

	header.appendChild(
		createNode({
			content: new waveforge.TextContent("WAVEFORGE LAYOUT TEST", 1, "#dcdcdc"),
		}),
	);

	// ── 2. Main content (side-by-side panels) ──
	const main = createNode({ flexGrow: 1, flexDirection: "row" });

	// left panel: column, center-aligned
	const leftPanel = createNode({
		flexGrow: 1,
		flexDirection: "column",
		alignItems: "center",
		justifyContent: "center",
		backgroundColor: "#3c404a",
	});

	leftPanel.appendChild(
		createNode({
			content: new waveforge.SpriteContent(duck, 2),
			marginBottom: 4,
		}),
	);

	leftPanel.appendChild(
		createNode({
			content: new waveforge.TextContent("rubber duck", 1, "#c8c8c8"),
		}),
	);

	// right panel: column, space-evenly items
	const rightPanel = createNode({
		flexGrow: 1,
		flexDirection: "column",
		justifyContent: "spaceEvenly",
		alignItems: "center",
		backgroundColor: "#323640",
	});

	const elements = ["sand", "water", "fire", "stone"];
	const elementColors = ["#d2be82", "#3c78b4", "#ff5028", "#8c8278"];
	for (let i = 0; i < elements.length; i++) {
		const row = createNode({ flexDirection: "row", alignItems: "center" });

		row.appendChild(
			createNode({
				width: 6,
				height: 6,
				backgroundColor: elementColors[i],
				marginRight: 1,
			}),
		);

		row.appendChild(
			createNode({
				content: new waveforge.TextContent(elements[i], 1, "#bebebe"),
			}),
		);

		rightPanel.appendChild(row);
	}

	main.appendChild(leftPanel);
	main.appendChild(rightPanel);

	// ── 3. Footer ──
	const footer = createNode({
		height: 18,
		backgroundColor: "#282c34",
		flexDirection: "row",
		alignItems: "center",
		justifyContent: "spaceAround",
	});

	const buttons = ["PLAY", "HELP", "EXIT"];
	for (const label of buttons) {
		footer.appendChild(
			createNode({
				content: new waveforge.TextContent(label, 1, "#b4c8f0"),
				paddingTop: 2,
				paddingRight: 6,
				paddingBottom: 2,
				paddingLeft: 6,
				backgroundColor: "#3c465a",
			}),
		);
	}

	root.appendChild(header);
	root.appendChild(main);
	root.appendChild(footer);

	return root;
}

waveforge.setupScene({
	size() {
		return [W, H];
	},

	setup() {
		layoutRoot = buildLayout();
		waveforge.log("layout test ready");
	},

	step() {
		frameCount++;
		fpsText.text = `FRAME: ${frameCount}`;
	},

	render() {
		waveforge.commitLayout(layoutRoot);
		waveforge.commitDraw(cmds);
	},
});
