import { render, Sprite, Text, View } from '@waveforge/renderer';
import {
	center,
	centerH,
	centerV,
	column,
	contentAlignH,
	contentAlignV,
	flex,
	padding,
	row,
	size,
	style,
	textColor,
} from '@waveforge/styles';
import React from 'react';

const duck = new waveforge.Texture('duck/texture');

const ELEMENTS = [
	{ name: 'sand', color: '#d2be82' },
	{ name: 'water', color: '#3c78b4' },
	{ name: 'fire', color: '#ff5028' },
	{ name: 'stone', color: '#8c8278' },
];

const BUTTON_LABELS = ['PLAY', 'HELP', 'EXIT'];

function App() {
	const [frameCount, setFrameCount] = React.useState(0);

	React.useEffect(() => {
		const handler = () => setFrameCount((c) => c + 1);
		waveforge.addEventListener('step', handler);
		return () => waveforge.removeEventListener('step', handler);
	}, []);

	const handleButtonClick = React.useCallback((label: string) => {
		console.log(`BUTTON "${label}" clicked!`);
	}, []);

	return (
		<View style={style(size(256, 192), column())}>
			{/* Header */}
			<View style={style({ height: 16, backgroundColor: '#282c34' }, row(), centerV())}>
				<View style={style(flex(1), centerH())}>
					<Text style={textColor('#dcdcdc')}>WAVEFORGE LAYOUT TEST</Text>
				</View>
				<Text style={textColor('#ffffff')}>{`F:${frameCount}`}</Text>
			</View>

			{/* Body */}
			<View style={style(flex(1), row())}>
				{/* Left panel */}
				<View style={style(flex(1), column(), center(), { backgroundColor: '#3c404a' })}>
					<Sprite texture={duck} scale={2} style={{ marginBottom: 4 }} />
					<Text style={textColor('#c8c8c8')}>rubber duck</Text>
				</View>

				{/* Right panel */}
				<View
					style={style(flex(1), column(), {
						backgroundColor: '#323640',
						justifyContent: 'spaceEvenly',
						alignItems: 'center',
					})}
				>
					{ELEMENTS.map(({ name, color }) => (
						<View key={name} style={row()}>
							<View style={style(size(6, 6), { backgroundColor: color, marginRight: 1 })} />
							<Text style={textColor('#bebebe')}>{name}</Text>
						</View>
					))}
				</View>
			</View>

			{/* Footer */}
			<View
				style={style({ height: 18, backgroundColor: '#282c34' }, row(), centerV(), { justifyContent: 'spaceAround' })}
			>
				{BUTTON_LABELS.map((label) => (
					<View
						key={label}
						onClick={() => handleButtonClick(label)}
						style={style(
							{ backgroundColor: '#3c465a' },
							padding(2, 6),
							contentAlignH('center'),
							contentAlignV('horizon'),
						)}
					>
						<Text style={textColor('#b4c8f0')}>{label}</Text>
					</View>
				))}
			</View>
		</View>
	);
}

render(<App />);
console.log('layout test ready');
