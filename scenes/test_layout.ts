const W = 256;
const H = 192;

let frameCount = 0;
let layoutRoot: waveforge.LayoutNode;

const fpsText = new waveforge.DrawTextCmd('FRAME: 0', 4, 160, 1, '#ffffff');

const duck = new waveforge.Texture('duck/texture');
const cmds = new waveforge.DrawCmdList();
cmds.push(fpsText);

function createNode(props: any): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	Object.assign(node, props);
	return node;
}

const buttonNodes: waveforge.LayoutNode[] = [];

function buildLayout(): waveforge.LayoutNode {
	const root = createNode({ width: W, height: H, flexDirection: 'column' });

	const header = createNode({
		height: 16,
		backgroundColor: '#282c34',
		flexDirection: 'row',
		alignItems: 'center',
		justifyContent: 'center',
	});

	header.appendChild(
		createNode({
			content: new waveforge.TextContent('WAVEFORGE LAYOUT TEST', 1, '#dcdcdc'),
		}),
	);

	const main = createNode({ flexGrow: 1, flexDirection: 'row' });

	const leftPanel = createNode({
		flexGrow: 1,
		flexDirection: 'column',
		alignItems: 'center',
		justifyContent: 'center',
		backgroundColor: '#3c404a',
	});

	leftPanel.appendChild(
		createNode({
			content: new waveforge.SpriteContent(duck, 2),
			marginBottom: 4,
		}),
	);

	leftPanel.appendChild(
		createNode({
			content: new waveforge.TextContent('rubber duck', 1, '#c8c8c8'),
		}),
	);

	const rightPanel = createNode({
		flexGrow: 1,
		flexDirection: 'column',
		justifyContent: 'spaceEvenly',
		alignItems: 'center',
		backgroundColor: '#323640',
	});

	const elements = ['sand', 'water', 'fire', 'stone'];
	const elementColors = ['#d2be82', '#3c78b4', '#ff5028', '#8c8278'];
	for (let i = 0; i < elements.length; i++) {
		const row = createNode({ flexDirection: 'row', alignItems: 'center' });

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
				content: new waveforge.TextContent(elements[i], 1, '#bebebe'),
			}),
		);

		rightPanel.appendChild(row);
	}

	main.appendChild(leftPanel);
	main.appendChild(rightPanel);

	const footer = createNode({
		height: 18,
		backgroundColor: '#282c34',
		flexDirection: 'row',
		alignItems: 'center',
		justifyContent: 'spaceAround',
	});

	const buttons = ['PLAY', 'HELP', 'EXIT'];
	for (const label of buttons) {
		const btn = createNode({
			content: new waveforge.TextContent(label, 1, '#b4c8f0'),
			paddingTop: 2,
			paddingRight: 6,
			paddingBottom: 2,
			paddingLeft: 6,
			backgroundColor: '#3c465a',
			contentAlignH: 'center',
			contentAlignV: 'horizon',
		});
		footer.appendChild(btn);
		buttonNodes.push(btn);
	}

	root.appendChild(header);
	root.appendChild(main);
	root.appendChild(footer);

	return root;
}

function handleClick(event: waveforge.MouseButtonEvent): void {
	console.log(`Click at (${event.x}, ${event.y})`);

	const target = layoutRoot.hitTest(event.x, event.y);
	if (!target) {
		console.log('  → no node hit');
		return;
	}

	const bounds = target.getComputedBounds();
	if (bounds) {
		console.log(`  → hit node bounds=(${bounds.x},${bounds.y} ${bounds.width}x${bounds.height})`);
	}

	console.log(`  → hasChildNodes: ${target.hasChildNodes()}`);

	const root = target.getRootNode();
	console.log(`  → root node === layoutRoot: ${root === layoutRoot}`);

	if (target.parent) {
		const pb = target.parent.getComputedBounds();
		if (pb) {
			console.log(`  → parent bounds=(${pb.x},${pb.y} ${pb.width}x${pb.height})`);
		}
	}

	const btnIndex = buttonNodes.indexOf(target);
	if (btnIndex >= 0) {
		console.log(`  → BUTTON "${['PLAY', 'HELP', 'EXIT'][btnIndex]}" clicked!`);
	}
}

layoutRoot = buildLayout();
console.log('layout test ready');
console.log(`root hasChildNodes: ${layoutRoot.hasChildNodes()}`);
console.log(`root childCount: ${layoutRoot.childCount}`);

waveforge.addEventListener('step', () => {
	frameCount++;
	fpsText.text = `FRAME: ${frameCount}`;
	waveforge.commitLayout(layoutRoot);
	waveforge.commitDraw(cmds);
});

waveforge.addEventListener('mousebutton', (event) => {
	handleClick(event);
});

export {};
