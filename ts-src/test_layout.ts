/// <reference path="./waveforge.d.ts" />

const W = 256;
const H = 192;

let frameCount = 0;
let layoutRoot: waveforge.LayoutNode;

const fpsText = new waveforge.DrawTextCmd("FPS: 0", 4, 178, 1,
    new waveforge.Color(100, 100, 100));

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
    header.backgroundColor = new waveforge.Color(40, 44, 52);
    header.flexDirection = "row";
    header.alignItems = "center";
    header.justifyContent = "center";

    const headText = new waveforge.LayoutNode();
    headText.content = new waveforge.TextContent("WAVEFORGE LAYOUT TEST", 1,
        new waveforge.Color(220, 220, 220));
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
    leftPanel.backgroundColor = new waveforge.Color(60, 64, 74);

    const duckNode = new waveforge.LayoutNode();
    duckNode.content = new waveforge.SpriteContent(duck, 2);
    leftPanel.appendChild(duckNode);

    const duckLabel = new waveforge.LayoutNode();
    duckLabel.content = new waveforge.TextContent("rubber duck", 1,
        new waveforge.Color(200, 200, 200));
    leftPanel.appendChild(duckLabel);

    // right panel: column, space-evenly items
    const rightPanel = new waveforge.LayoutNode();
    rightPanel.flexGrow = 1;
    rightPanel.flexDirection = "column";
    rightPanel.justifyContent = "spaceEvenly";
    rightPanel.alignItems = "center";
    rightPanel.backgroundColor = new waveforge.Color(50, 54, 64);

    const elements = ["sand", "water", "fire", "stone"];
    const elementColors = [
        [210, 190, 130], [60, 120, 180],
        [255, 80, 40], [140, 130, 120],
    ];
    for (let i = 0; i < elements.length; i++) {
        const row = new waveforge.LayoutNode();
        row.flexDirection = "row";
        row.alignItems = "center";

        const dot = new waveforge.LayoutNode();
        dot.width = 6; dot.height = 6;
        dot.backgroundColor = new waveforge.Color(
            elementColors[i][0], elementColors[i][1], elementColors[i][2]);
        dot.margin = { right: 4 };
        row.appendChild(dot);

        const label = new waveforge.LayoutNode();
        label.content = new waveforge.TextContent(elements[i], 1,
            new waveforge.Color(190, 190, 190));
        row.appendChild(label);

        rightPanel.appendChild(row);
    }

    main.appendChild(leftPanel);
    main.appendChild(rightPanel);

    // ── 3. Footer ──
    const footer = new waveforge.LayoutNode();
    footer.height = 18;
    footer.backgroundColor = new waveforge.Color(40, 44, 52);
    footer.flexDirection = "row";
    footer.alignItems = "center";
    footer.justifyContent = "spaceAround";

    const buttons = ["PLAY", "HELP", "EXIT"];
    for (const label of buttons) {
        const btn = new waveforge.LayoutNode();
        btn.content = new waveforge.TextContent(label, 1, new waveforge.Color(180, 200, 240));
        btn.padding = { top: 2, right: 6, bottom: 2, left: 6 };
        btn.backgroundColor = new waveforge.Color(60, 70, 90);
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
