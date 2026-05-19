// Must be first: timer globals needed by scenes that use setTimeout etc.

const WIDTH = 256;
const HEIGHT = 192;
let frameCount = 0;
let x = 50,
	y = 50,
	dx = 2,
	dy = 1;

const duck = new waveforge.Texture('duck/texture');
const cmds = new waveforge.DrawCmdList();
const drawDuckCmd = new waveforge.DrawSpriteCmd(duck, x, y);
const bgRect = new waveforge.DrawRectCmd(0, 0, WIDTH, HEIGHT, '#c8dcff');
const redRect = new waveforge.DrawRectCmd(10, 10, 40, 30, '#ff6464');
const fpsText = new waveforge.DrawTextCmd('FRAME: 0', 4, 4, 1, '#000000');

bgRect.color = '#dcf0ff';

cmds.push(bgRect);
cmds.push(redRect);
cmds.push(drawDuckCmd);
cmds.push(fpsText);

console.log(`Loaded ${duck.width}x${duck.height} duck texture`);
console.log('Test scene loading...');
queueMicrotask(() => {
	console.log('Microtask executed');
});

// setTimeout demo: after 2s, turn background red
waveforge.addEventListener('step', () => {
	frameCount++;
	x += dx;
	y += dy;
	if (x <= 0 || x >= WIDTH - duck.width) {
		dx = -dx;
	}
	if (y <= 0 || y >= HEIGHT - duck.height) {
		dy = -dy;
	}
	drawDuckCmd.x = x;
	drawDuckCmd.y = y;
	fpsText.text = `FRAME: ${frameCount}`;
	if (frameCount % 24 === 0) {
		console.log(`Frame ${frameCount}: ${performance.now()} ms`);
	}
	waveforge.commitDraw(cmds);
});

waveforge.addEventListener('key', (event) => {
	console.log(`Key: ${event.code} ${event.type}`);
});

waveforge.addEventListener('mousebutton', (event) => {
	console.log(`Mouse ${event.type} at (${event.x}, ${event.y})`);
});

export {};
