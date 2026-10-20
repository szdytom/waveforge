import { Hint, Label, playUiSound, Screen, Title, useMenuNavigation } from '@waveforge/components';
import { render, Sprite, Text, View } from '@waveforge/renderer';
import { center, fill, style } from '@waveforge/styles';
import React from 'react';

const BUTTON_X = [16, 80, 144, 208];
const LINK_X = [-16, 48, 112, 176, 240];
const BUTTON_Y = 75;
const levelTextures = waveforge.levels.map((level) => new waveforge.Texture(level.minimapId));
const lockedTexture = new waveforge.Texture('ui/level-button-locked');
const selectedTexture = new waveforge.Texture('ui/level-button-selected-frame');
const linkTexture = new waveforge.Texture('ui/level-link');
const duckTexture = new waveforge.Texture('duck/texture');

function LevelMenuScene() {
	const completedLevels = Math.max(0, waveforge.storage.getInt('completed_levels'));
	const availableCount = Math.min(waveforge.levels.length, completedLevels + 1);
	const activate = React.useCallback((index: number) => {
		const level = waveforge.levels[index];
		if (!level) {
			return;
		}
		playUiSound('forward');
		const route = waveforge.storage.getBool('settings.skip_animations') ? 'play-level' : 'load-level';
		waveforge.navigateTo(route, level.id);
	}, []);
	const goBack = React.useCallback(() => {
		playUiSound('backward');
		waveforge.navigateTo('main-menu');
	}, []);
	const { selectedIndex, setSelectedIndex } = useMenuNavigation({
		itemCount: availableCount,
		initialIndex: Math.min(completedLevels, availableCount - 1),
		onActivate: activate,
		onBack: goBack,
		orientation: 'horizontal',
		wrap: false,
	});

	React.useEffect(() => {
		waveforge.music.setCollection('background/main-menu-music');
	}, []);

	const idealDuckButtonIndex = Math.floor((BUTTON_X.length - 1) / 2);
	let duckButtonIndex = idealDuckButtonIndex;
	if (selectedIndex < idealDuckButtonIndex) {
		duckButtonIndex = selectedIndex;
	} else if (selectedIndex + (BUTTON_X.length - idealDuckButtonIndex) > waveforge.levels.length) {
		duckButtonIndex = BUTTON_X.length - (waveforge.levels.length - selectedIndex);
	}
	const firstLevel = selectedIndex - duckButtonIndex;
	const visibleLevels = waveforge.levels.slice(firstLevel, firstLevel + BUTTON_X.length);
	const selectedLevel = waveforge.levels[selectedIndex];

	return (
		<Screen title="Level Menu">
			<Title style={{ position: 'absolute', left: 16, top: 12 }} size={4}>
				Levels
			</Title>
			{LINK_X.map((left, slot) => {
				const levelIndex = firstLevel + slot;
				if (levelIndex <= 0 || levelIndex >= waveforge.levels.length) {
					return null;
				}
				return (
					<Sprite
						key={`link-${levelIndex}`}
						texture={linkTexture}
						style={{ position: 'absolute', left, top: BUTTON_Y }}
					/>
				);
			})}
			{visibleLevels.map((level, slot) => {
				const locked = level.index > completedLevels;
				const selected = level.index === selectedIndex;
				return (
					<View
						key={level.id}
						onClick={locked ? undefined : () => (selected ? activate(level.index) : setSelectedIndex(level.index))}
						onPointerEnter={locked ? undefined : () => setSelectedIndex(level.index)}
						style={style(center(), {
							position: 'absolute',
							left: BUTTON_X[slot],
							top: BUTTON_Y,
							width: 32,
							height: 32,
						})}
					>
						<Sprite
							texture={locked ? lockedTexture : levelTextures[level.index]}
							style={style(fill(), { position: 'absolute', left: 0, top: 0 })}
						/>
						{locked ? null : (
							<Text
								color="#ffffff"
								size={2}
								style={{
									position: 'absolute',
									left: 1 + Math.floor((31 - `${level.index + 1}`.length * 11) / 2),
									top: 9,
								}}
							>
								{`${level.index + 1}`}
							</Text>
						)}
						{selected ? (
							<Sprite texture={selectedTexture} style={style(fill(), { position: 'absolute', left: 0, top: 0 })} />
						) : null}
					</View>
				);
			})}
			<Sprite texture={duckTexture} style={{ position: 'absolute', left: BUTTON_X[duckButtonIndex] + 10, top: 64 }} />
			{selectedLevel ? (
				<>
					<Label size={2} style={{ position: 'absolute', left: 16, top: 122 }}>
						{selectedLevel.name}
					</Label>
					<Hint style={{ position: 'absolute', left: 16, top: 141 }}>{selectedLevel.description}</Hint>
					<Hint style={{ position: 'absolute', left: 16, top: 153 }}>{`Difficulty:${selectedLevel.difficulty}`}</Hint>
					<Label color="#fac82e" style={{ position: 'absolute', left: 16, top: 165 }}>
						Play
					</Label>
					<Hint color="#b4b4b4" style={{ position: 'absolute', left: 40, top: 165 }}>
						[Enter]
					</Hint>
				</>
			) : null}
		</Screen>
	);
}

render(<LevelMenuScene />);
