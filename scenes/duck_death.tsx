import { useKeyDown, useStep } from '@waveforge/components';
import { render, Sprite, View } from '@waveforge/renderer';
import { fill } from '@waveforge/styles';
import React from 'react';

const TOTAL_DURATION = 140;
const ANIMATION_START = 60;
const FRAME_DURATION = 3;
const FRAME_SIZE = 16;
const FRAME_COUNT = 12;
const DUCK_WIDTH = 13;
const DUCK_HEIGHT = 11;
const SEPARATE_SOUND_START = 1;
const REBORN_SOUND_START = 36;
const animationTexture = new waveforge.Texture('duckdeath/texture');
const separateSound = new waveforge.Sound('sfx/duckdeath-separate');
const rebornSound = new waveforge.Sound('sfx/duckdeath');

interface DeathState {
	levelId: string;
	width: number;
	height: number;
	duckX: number;
	duckY: number;
	failed: boolean;
}

function DuckDeathScene() {
	const state = React.useMemo<DeathState>(() => {
		if (!waveforge.routeData) {
			throw new Error('Duck death scene requires transition state');
		}
		return JSON.parse(waveforge.routeData);
	}, []);
	const [tick, setTick] = React.useState(waveforge.storage.getBool('settings.skip_animations') ? TOTAL_DURATION : 0);

	React.useEffect(() => {
		waveforge.music.unset();
		return () => {
			separateSound.stop();
			rebornSound.stop();
		};
	}, []);

	useKeyDown((event) => {
		if (event.code === 'Space' || event.code === 'Enter' || event.code === 'r') {
			setTick(TOTAL_DURATION);
		}
	});
	useStep(() => {
		const next = tick + 1;
		if (next > TOTAL_DURATION) {
			waveforge.navigateTo('play-level', state.levelId);
			return;
		}
		if (next === SEPARATE_SOUND_START) {
			separateSound.play();
		}
		if (next === REBORN_SOUND_START) {
			rebornSound.play();
		}
		setTick(next);
	});

	const frame =
		tick < ANIMATION_START ? 0 : Math.min(FRAME_COUNT - 1, Math.floor((tick - ANIMATION_START) / FRAME_DURATION) + 1);
	const duckX = state.failed ? Math.floor((state.width - DUCK_WIDTH) / 2) : state.duckX;
	const duckY = state.failed ? Math.floor((state.height - DUCK_HEIGHT) / 2) : state.duckY;

	return (
		<View style={{ ...fill(), backgroundColor: '#dcdcdc' }}>
			<Sprite
				texture={animationTexture}
				sourceX={frame * FRAME_SIZE}
				sourceWidth={FRAME_SIZE}
				sourceHeight={FRAME_SIZE}
				style={{ position: 'absolute', left: duckX - 2, top: duckY - 2 }}
			/>
		</View>
	);
}

render(<DuckDeathScene />);
