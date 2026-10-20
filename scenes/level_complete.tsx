import { Label, useStep } from '@waveforge/components';
import { render, Sprite, View } from '@waveforge/renderer';
import { fill } from '@waveforge/styles';
import React from 'react';

const MESSAGE = 'LEVEL COMPLETED!';
const DUCK_WIDTH = 13;
const DUCK_HEIGHT = 11;
const FONT_WIDTH = 6;
const PENDING_TICKS = 24;
const MOVE_SPEED = 4;
const duckTexture = new waveforge.Texture('duck/texture');
const completeSound = new waveforge.Sound('sfx/level-complete');

interface CompleteState {
	width: number;
	height: number;
	duckX: number;
	duckY: number;
}

interface Point {
	x: number;
	y: number;
}

function pointsOnSegment(start: Point, end: Point): Point[] {
	const points: Point[] = [];
	let x = start.x;
	let y = start.y;
	const dx = Math.abs(end.x - x);
	const sx = x < end.x ? 1 : -1;
	const dy = -Math.abs(end.y - y);
	const sy = y < end.y ? 1 : -1;
	let error = dx + dy;
	while (true) {
		points.push({ x, y });
		if (x === end.x && y === end.y) {
			return points;
		}
		const doubledError = 2 * error;
		if (doubledError >= dy) {
			error += dy;
			x += sx;
		}
		if (doubledError <= dx) {
			error += dx;
			y += sy;
		}
	}
}

function LevelCompleteScene() {
	const state = React.useMemo<CompleteState>(() => {
		if (!waveforge.routeData) {
			throw new Error('Level completion scene requires transition state');
		}
		return JSON.parse(waveforge.routeData);
	}, []);
	const target = React.useMemo(
		() => ({
			x: Math.floor((state.width - (MESSAGE.length * FONT_WIDTH + 3 + DUCK_WIDTH)) / 2),
			y: Math.floor((state.height - DUCK_HEIGHT) / 2),
		}),
		[state],
	);
	const positions = React.useMemo(() => {
		const path = pointsOnSegment({ x: state.duckX, y: state.duckY }, target).filter(
			(_point, index) => index % MOVE_SPEED === 0,
		);
		const last = path[path.length - 1];
		if (!last || last.x !== target.x || last.y !== target.y) {
			path.push(target);
		}
		return path;
	}, [state, target]);
	const skipAnimation = waveforge.storage.getBool('settings.skip_animations');
	const [positionIndex, setPositionIndex] = React.useState(skipAnimation ? positions.length - 1 : 0);
	const [pendingTicks, setPendingTicks] = React.useState(0);
	const [displayMessage, setDisplayMessage] = React.useState(skipAnimation);

	React.useEffect(() => {
		waveforge.music.unset();
		if (!skipAnimation) {
			completeSound.play();
		}
		return () => completeSound.stop();
	}, [skipAnimation]);

	useStep(() => {
		if (pendingTicks < PENDING_TICKS) {
			setPendingTicks(pendingTicks + 1);
			return;
		}
		if (displayMessage) {
			waveforge.navigateTo('level-menu');
			return;
		}
		const nextPosition = Math.min(positionIndex + 1, positions.length - 1);
		setPositionIndex(nextPosition);
		if (nextPosition === positions.length - 1) {
			setPendingTicks(0);
			setDisplayMessage(true);
		}
	});

	const position = positions[positionIndex];
	return (
		<View style={{ ...fill(), backgroundColor: '#ffffff' }}>
			<Sprite texture={duckTexture} style={{ position: 'absolute', left: position.x, top: position.y }} />
			{displayMessage ? (
				<Label
					style={{
						position: 'absolute',
						left: target.x + DUCK_WIDTH + 3,
						top: Math.floor((state.height - 6) / 2) - 1,
					}}
				>
					{MESSAGE}
				</Label>
			) : null}
		</View>
	);
}

render(<LevelCompleteScene />);
