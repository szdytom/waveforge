import { MenuItem, playUiSound, Screen, useMenuNavigation } from '@waveforge/components';
import { render, Sprite, Text, View } from '@waveforge/renderer';
import { column, fill, style } from '@waveforge/styles';
import React from 'react';

const backgroundTexture = new waveforge.Texture('ui/main-menu-background');

interface MenuAction {
	label: string;
	run(): void;
}

function MainMenuScene() {
	const actions = React.useMemo<MenuAction[]>(
		() => [
			{
				label: waveforge.storage.getInt('completed_levels') === 0 ? 'New Game' : 'Play',
				run: () => waveforge.navigateTo('level-menu'),
			},
			{ label: 'Settings', run: () => waveforge.navigateTo('settings') },
			{ label: 'Help', run: () => waveforge.navigateTo('help') },
			{ label: 'Exit', run: () => waveforge.close() },
		],
		[],
	);

	const activate = React.useCallback(
		(index: number) => {
			playUiSound('forward');
			actions[index]?.run();
		},
		[actions],
	);
	const { selectedIndex, setSelectedIndex } = useMenuNavigation({
		itemCount: actions.length,
		onActivate: activate,
		onBack: () => waveforge.close(),
	});

	React.useEffect(() => {
		waveforge.music.setCollection('background/main-menu-music');
	}, []);

	return (
		<Screen title={`Waveforge ${waveforge.version}`}>
			<Sprite texture={backgroundTexture} style={fill()} />
			<View
				style={style(column(), {
					position: 'absolute',
					left: 68,
					top: 76,
					width: 128,
					gap: 4,
				})}
			>
				{actions.map((action, index) => (
					<MenuItem
						key={action.label}
						label={action.label}
						selected={selectedIndex === index}
						onSelect={() => setSelectedIndex(index)}
						onActivate={() => activate(index)}
						textSize={2}
						style={{ height: 20 }}
					/>
				))}
			</View>
			<Text color="#00000028" style={{ position: 'absolute', left: 1, bottom: 1 }}>
				{`V${waveforge.version}${waveforge.commitHash ? `-${waveforge.commitHash}` : ''}`}
			</Text>
		</Screen>
	);
}

render(<MainMenuScene />);
