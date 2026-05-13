/// <reference path="./waveforge.d.ts" />

const W = 256;
const H = 192;

let frameCount = 0;
let layoutRoot: waveforge.LayoutNode;

const fpsText = new waveforge.DrawTextCmd("FPS: 0", 4, 178, 1,
    "#646464");

const duck = new waveforge.Texture("duck/texture");
const cmds = new waveforge.DrawCmdList();
cmds.push(fpsText);

function buildLayout(): waveforge.LayoutNode {
    const root = new waveforge.LayoutNode();
    root.width = W;
    root.height = H;
    root.flexDirection = "column";

    // ── 1. Header ──
    const header = new waveforge.LayoutNode();
    header.height = 16;
    header.backgroundColor = "#282c34";
    header.flexDirection = "row";
    header.alignItems = "center";
    header.justifyContent = "center";

    const headText = new waveforge.LayoutNode();
    headText.content = new waveforge.TextContent("WAVEFORGE LAYOUT TEST", 1,
        "#dcdcdc");
    header.appendChild(headText);

    // ── 2. Main content (side-by-side panels) ──
    const main = new waveforge.LayoutNode();
    main.flexGrow = 1;
    main.flexDirection = "row";

    // left panel: column, center-aligned
    const leftPanel = new waveforge.LayoutNode();
    leftPanel.flexGrow = 1;
    leftPanel.flexDirection = "column";
    leftPanel.alignItems = "center";
    leftPanel.justifyContent = "center";
    leftPanel.backgroundColor = "#3c404a";

    const duckNode = new waveforge.LayoutNode();
    duckNode.content = new waveforge.SpriteContent(duck, 2);
	duckNode.marginBottom = 4;
    leftPanel.appendChild(duckNode);

    const duckLabel = new waveforge.LayoutNode();
    duckLabel.content = new waveforge.TextContent("rubber duck", 1,
        "#c8c8c8");
    leftPanel.appendChild(duckLabel);

    // right panel: column, space-evenly items
    const rightPanel = new waveforge.LayoutNode();
    rightPanel.flexGrow = 1;
    rightPanel.flexDirection = "column";
    rightPanel.justifyContent = "spaceEvenly";
    rightPanel.alignItems = "center";
    rightPanel.backgroundColor = "#323640";

    const elements = ["sand", "water", "fire", "stone"];
    const elementColors = [
        "#d2be82", "#3c78b4",
        "#ff5028", "#8c8278",
    ];
    for (let i = 0; i < elements.length; i++) {
        const row = new waveforge.LayoutNode();
        row.flexDirection = "row";
        row.alignItems = "center";

        const dot = new waveforge.LayoutNode();
        dot.width = 6;
		dot.height = 6;
        dot.backgroundColor = elementColors[i];
        dot.marginRight = 1;
        row.appendChild(dot);

        const label = new waveforge.LayoutNode();
        label.content = new waveforge.TextContent(elements[i], 1, "#bebebe");
        row.appendChild(label);

        rightPanel.appendChild(row);
    }

    main.appendChild(leftPanel);
    main.appendChild(rightPanel);

    // ── 3. Footer ──
    const footer = new waveforge.LayoutNode();
    footer.height = 18;
    footer.backgroundColor = "#282c34";
    footer.flexDirection = "row";
    footer.alignItems = "center";
    footer.justifyContent = "spaceAround";

    const buttons = ["PLAY", "HELP", "EXIT"];
    for (const label of buttons) {
        const btn = new waveforge.LayoutNode();
        btn.content = new waveforge.TextContent(label, 1, "#b4c8f0");
        btn.paddingTop = 2;
        btn.paddingRight = 6;
        btn.paddingBottom = 2;
        btn.paddingLeft = 6;
        btn.backgroundColor = "#3c465a";
        footer.appendChild(btn);
    }

    root.appendChild(header);
    root.appendChild(main);
    root.appendChild(footer);

    return root;
}

waveforge.setupScene({
    size() { return [W, H]; },

    setup() {
        layoutRoot = buildLayout();
        waveforge.log("layout test ready");
    },

    step() {
        frameCount++;
        fpsText.text = "FPS: " + frameCount;
    },

    render() {
        waveforge.commitLayout(layoutRoot);
        waveforge.commitDraw(cmds);
    },
});
