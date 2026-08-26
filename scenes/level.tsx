import { Label, MenuItem, playUiSound, useEngineEvent, useKeyDown, useStep } from '@waveforge/components';
import { Level as LevelView, render, Sprite, View } from '@waveforge/renderer';
import { center, column, fill, style } from '@waveforge/styles';
import React from 'react';

const RETRY_COOLDOWN_TICKS = 48;
const HINT_MAX_OPACITY = 200;
const HINT_FADE_SPEED = 3;
const PAUSE_ACTIONS = ['Resume', 'Retry', 'Help', 'Quit'] as const;
const helpTexture = new waveforge.Texture('ui/help');

type HintType = 'restart' | null;

function LevelScene() {
	const levelId = waveforge.routeData;
	if (!levelId) {
		throw new Error('Level scene requires a level id');
	}

	const [surface] = React.useState(() => new waveforge.LevelSurface(levelId));
	const [paused, setPaused] = React.useState(false);
	const [showHelp, setShowHelp] = React.useState(false);
	const [pauseIndex, setPauseIndex] = React.useState(0);
	const [hintType, setHintType] = React.useState<HintType>(null);
	const [hintOpacity, setHintOpacity] = React.useState(0);
	const tickRef = React.useRef(0);
	const transitioningRef = React.useRef(false);

	const restart = React.useCallback(
		(failed: boolean) => {
			if (transitioningRef.current) {
				return;
			}
			transitioningRef.current = true;
			waveforge.navigateTo(
				'level-death',
				JSON.stringify({
					levelId: surface.levelId,
					width: surface.width,
					height: surface.height,
					duckX: surface.duckX,
					duckY: surface.duckY,
					failed,
				}),
			);
		},
		[surface],
	);

	const complete = React.useCallback(() => {
		if (transitioningRef.current) {
			return;
		}
		transitioningRef.current = true;
		const completedLevels = waveforge.storage.getInt('completed_levels');
		waveforge.storage.setInt('completed_levels', Math.max(completedLevels, surface.levelIndex + 1));
		waveforge.navigateTo(
			'level-complete',
			JSON.stringify({
				width: surface.width,
				height: surface.height,
				duckX: surface.duckX,
				duckY: surface.duckY,
			}),
		);
	}, [surface]);

	React.useEffect(() => {
		waveforge.setTitle(`Level ${surface.levelIndex + 1} - ${surface.levelName}`);
		waveforge.music.setCollection('background/level-music');
		waveforge.music.fadeIn(48, 0.2);
		return () => waveforge.music.setVolume(1);
	}, [surface]);

	React.useEffect(() => {
		waveforge.music.setVolume(paused ? 0.15 : 1);
	}, [paused]);

	useStep(() => {
		if (!paused && !transitioningRef.current) {
			tickRef.current += 1;
			surface.step();
			if (surface.failed) {
				restart(true);
				return;
			}
			if (surface.completed) {
				complete();
				return;
			}
		}

		setHintOpacity((opacity) => {
			if (opacity === 0) {
				return 0;
			}
			const next = Math.max(0, opacity - HINT_FADE_SPEED);
			return next;
		});
	});

	const activatePauseAction = React.useCallback(
		(index: number) => {
			switch (PAUSE_ACTIONS[index]) {
				case 'Resume':
					setPaused(false);
					break;
				case 'Retry':
					restart(false);
					break;
				case 'Help':
					setShowHelp(true);
					break;
				case 'Quit':
					waveforge.navigateTo('level-menu');
					break;
			}
		},
		[restart],
	);

	useKeyDown((event) => {
		if (paused) {
			if (showHelp) {
				if (event.code === 'Escape' || event.code === 'Enter' || event.code === 'Space') {
					playUiSound('backward');
					setShowHelp(false);
				}
				return;
			}
			if (event.code === 'Escape') {
				setPaused(false);
			} else if (event.code === 'ArrowUp' || event.code === 'w') {
				playUiSound('backward');
				setPauseIndex((index) => (index + PAUSE_ACTIONS.length - 1) % PAUSE_ACTIONS.length);
			} else if (event.code === 'ArrowDown' || event.code === 's') {
				playUiSound('forward');
				setPauseIndex((index) => (index + 1) % PAUSE_ACTIONS.length);
			} else if (event.code === 'Enter' || event.code === 'Space') {
				activatePauseAction(pauseIndex);
			}
			return;
		}

		if (/^[1-9]$/.test(event.code)) {
			surface.selectItem(Number(event.code) - 1);
		} else if (event.code === 'Escape') {
			setPaused(true);
		} else if (event.code === 'ArrowUp' || event.code === 'PageUp' || event.code === 'w') {
			surface.prevItem();
		} else if (event.code === 'ArrowDown' || event.code === 'PageDown' || event.code === 's') {
			surface.nextItem();
		} else if (event.code === 'r' && tickRef.current >= RETRY_COOLDOWN_TICKS) {
			if (hintType === 'restart' && hintOpacity > 0) {
				restart(false);
			} else {
				setHintType('restart');
				setHintOpacity(HINT_MAX_OPACITY);
			}
		}
	});

	useEngineEvent('mousemove', (event) => surface.setPointer(event.x, event.y));
	useEngineEvent('mousebutton', (event) => {
		if (!paused && event.type === 'mousedown' && event.button === 0) {
			surface.applyItem(event.x, event.y);
		}
	});
	useEngineEvent('mousewheel', (event) => {
		if (!paused && event.delta !== 0) {
			surface.changeBrushSize(event.delta > 0 ? 1 : -1);
		}
	});

	return (
		<View style={fill()}>
			<LevelView surface={surface} style={fill()} />
			{hintType === 'restart' && hintOpacity > 0 ? (
				<Label color={[31, 27, 45, hintOpacity]} style={{ position: 'absolute', bottom: 10, alignSelf: 'center' }}>
					Press R again to retry
				</Label>
			) : null}
			{paused ? (
				<View style={style(fill(), center(), { position: 'absolute', backgroundColor: '#000000c8' })}>
					<View style={style(column(), { width: 100, gap: 3 })}>
						<Label color="#ffffff" size={2} style={{ alignSelf: 'center', marginBottom: 14 }}>
							PAUSED
						</Label>
						{PAUSE_ACTIONS.map((label, index) => (
							<MenuItem
								key={label}
								label={label}
								selected={pauseIndex === index}
								color="#ffffff"
								onSelect={() => setPauseIndex(index)}
								onActivate={() => activatePauseAction(index)}
							/>
						))}
					</View>
				</View>
			) : null}
			{showHelp ? <Sprite texture={helpTexture} style={{ ...fill(), position: 'absolute' }} /> : null}
		</View>
	);
}

render(<LevelScene />);
