import {
	Column,
	Hint,
	MenuItem,
	playUiSound,
	Screen,
	Title,
	useKeyDown,
	useMenuNavigation,
	useStep,
} from '@waveforge/components';
import { render } from '@waveforge/renderer';
import React from 'react';

const HINT_MAX_OPACITY = 200;
const HINT_FADE_SPEED = 3;

const SETTINGS_KEYS = [
	'settings.scale',
	'settings.global_volume',
	'settings.strict_pixel_perfection',
	'settings.skip_animations',
	'settings.debug_heat_render',
] as const;

interface SettingOption {
	label: string;
	value?: string;
	decrease?(): void;
	increase?(): void;
	activate?(): void;
}

function SettingsScene() {
	const [, rerender] = React.useReducer((current) => current + 1, 0);
	const [resetConfirmed, setResetConfirmed] = React.useState(false);
	const [cheatProgress, setCheatProgress] = React.useState('');
	const [cheatOpacity, setCheatOpacity] = React.useState(0);

	const goBack = React.useCallback(() => {
		playUiSound('backward');
		waveforge.navigateTo('main-menu');
	}, []);

	const options: SettingOption[] = (() => {
		const updateInt = (key: string, value: number) => {
			waveforge.storage.setInt(key, value);
			rerender();
		};
		const updateBool = (key: string, value: boolean) => {
			waveforge.storage.setBool(key, value);
			rerender();
		};
		const scale = waveforge.storage.getInt('settings.scale');
		const volume = waveforge.storage.getInt('settings.global_volume', 80);
		const pixelPerfect = waveforge.storage.getBool('settings.strict_pixel_perfection');
		const skipAnimations = waveforge.storage.getBool('settings.skip_animations');
		const settings: SettingOption[] = [
			{
				label: 'Scale',
				value: scale === 0 ? 'Auto' : `${scale}x`,
				decrease: () => updateInt('settings.scale', Math.max(0, scale - 1)),
				increase: () => updateInt('settings.scale', Math.min(12, scale + 1)),
			},
			{
				label: 'Volume',
				value: volume === 0 ? 'Mute' : `${volume}`,
				decrease: () => updateInt('settings.global_volume', Math.max(0, volume - 5)),
				increase: () => updateInt('settings.global_volume', Math.min(100, volume + 5)),
			},
			{
				label: 'Pixel Perfect Rendering',
				value: pixelPerfect ? 'On' : 'Off',
				decrease: () => updateBool('settings.strict_pixel_perfection', false),
				increase: () => updateBool('settings.strict_pixel_perfection', true),
			},
			{
				label: 'Skip Animations',
				value: skipAnimations ? 'On' : 'Off',
				decrease: () => updateBool('settings.skip_animations', false),
				increase: () => updateBool('settings.skip_animations', true),
			},
		];
		if (waveforge.debug) {
			const heat = waveforge.storage.getBool('settings.debug_heat_render');
			settings.push({
				label: 'Debug Heat Render',
				value: heat ? 'On' : 'Off',
				decrease: () => updateBool('settings.debug_heat_render', false),
				increase: () => updateBool('settings.debug_heat_render', true),
			});
		}
		settings.push(
			{
				label: 'Reset Settings',
				activate: () => {
					for (const key of SETTINGS_KEYS) {
						waveforge.storage.remove(key);
					}
					rerender();
				},
			},
			{
				label: resetConfirmed ? 'Reset All(Again to Confirm)' : 'Reset All',
				activate: () => {
					if (!resetConfirmed) {
						setResetConfirmed(true);
						return;
					}
					waveforge.storage.remove('completed_levels');
					for (const key of SETTINGS_KEYS) {
						waveforge.storage.remove(key);
					}
					waveforge.navigateTo('main-menu');
				},
			},
			{ label: 'Back(esc)', activate: goBack },
		);
		return settings;
	})();

	const activate = React.useCallback(
		(index: number) => {
			playUiSound('forward');
			const option = options[index];
			if (option?.activate) {
				option.activate();
			} else {
				option?.increase?.();
			}
		},
		[options],
	);
	const adjust = React.useCallback(
		(index: number, direction: -1 | 1) => {
			playUiSound(direction < 0 ? 'backward' : 'forward');
			const option = options[index];
			if (direction < 0) {
				option?.decrease?.();
			} else {
				option?.increase?.();
			}
		},
		[options],
	);
	const { selectedIndex, setSelectedIndex } = useMenuNavigation({
		itemCount: options.length,
		onActivate: activate,
		onAdjust: adjust,
		onBack: goBack,
		wrap: false,
	});

	useKeyDown((event) => {
		if (event.code !== 'x' && event.code !== 'y' && event.code !== 'z') {
			return;
		}
		const next = `${cheatProgress}${event.code.toUpperCase()}`;
		if ('XYZZY'.startsWith(next)) {
			setCheatProgress(next);
			setCheatOpacity(HINT_MAX_OPACITY);
			if (next === 'XYZZY') {
				waveforge.storage.setInt('completed_levels', waveforge.levels.length);
			}
		} else {
			setCheatProgress(event.code === 'x' ? 'X' : '');
		}
	});

	useStep(() => {
		if (cheatOpacity > 0) {
			setCheatOpacity(Math.max(0, cheatOpacity - HINT_FADE_SPEED));
		} else if (cheatProgress) {
			setCheatProgress('');
		}
	});

	return (
		<Screen title="Settings">
			<Title style={{ position: 'absolute', left: 16, top: 14 }}>Settings</Title>
			<Hint style={{ position: 'absolute', left: 16, top: 31 }}>some changes require restart</Hint>
			<Column style={{ position: 'absolute', left: 32, top: 63, width: 192, gap: 6 }}>
				{options.map((option, index) => (
					<MenuItem
						key={option.label}
						label={option.label}
						value={option.value}
						selected={selectedIndex === index}
						onSelect={() => setSelectedIndex(index)}
						onActivate={() => activate(index)}
						style={{ height: 6, paddingLeft: 0, paddingRight: 0, paddingTop: 0, paddingBottom: 0 }}
					/>
				))}
			</Column>
			{cheatProgress && cheatOpacity > 0 ? (
				<Hint color={[0, 0, 0, cheatOpacity]} style={{ position: 'absolute', bottom: 11, alignSelf: 'center' }}>
					{cheatProgress === 'XYZZY' ? 'All levels unlocked' : cheatProgress}
				</Hint>
			) : null}
		</Screen>
	);
}

render(<SettingsScene />);
