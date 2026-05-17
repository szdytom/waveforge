// Must be first: timer globals needed by react-reconciler's scheduler
import React from 'react';
import { render, View, Text } from '@waveforge/renderer';
import { Button } from '@waveforge/components';
import { style, size, padding, margin, border, bg, row, column, center, centerV, centerH, flex, textColor } from '@waveforge/styles';

waveforge.log("Counter demo loading...");

const BG_COLORS = ['#f0e6ff', '#e6f5ff', '#fff0e6', '#e6ffe6', '#fffae6'];

const headerStyle = style(
	{ height: 18, backgroundColor: '#2d2d5e' },
	row(), centerV(), centerH(),
	border(0, '#4ecdc455'), { borderBottom: 1 },
);

const counterCard = style(
	bg('#ffffffcc'),
	padding(6, 24),
	border(2, '#4ecdc4'),
);

const footerStyle = style(
	{ height: 12, backgroundColor: '#2d2d5e' },
	row(), centerV(), centerH(),
	border(0, '#4ecdc455'), { borderTop: 1 },
);

function App() {
	const [count, setCount] = React.useState(0);
	const [bgIndex, setBgIndex] = React.useState(0);

	const increment = React.useCallback((n: number) => {
		setCount(c => {
			const next = c + n;
			setBgIndex(Math.floor(Math.abs(next) / 5) % BG_COLORS.length);
			return next;
		});
	}, []);

	const reset = React.useCallback(() => {
		setCount(0);
		setBgIndex(0);
	}, []);

	return (
		<View style={style(size(256, 192), column(), bg(BG_COLORS[bgIndex]))}>
			<View style={headerStyle}>
				<Text style={textColor('#ffffff')}>REACT COUNTER DEMO</Text>
			</View>

			<View style={style(flex(1), column(), center())}>
				<View style={style(counterCard, { marginBottom: 10 })}>
					<Text style={textColor('#2d2d5e')}>{`COUNT: ${count}`}</Text>
				</View>

				<View style={row()}>
					<Button label="+1" variant="primary" onClick={() => increment(1)} style={margin(0, 4)} />
					<Button label="+5" variant="accent" onClick={() => increment(5)} style={margin(0, 4)} />
					<Button label="RESET" variant="secondary" onClick={reset} style={margin(0, 4)} />
				</View>

				<View style={{ height: 6 }} />

				<Text style={textColor('#555')}>CLICK +1 OR +5</Text>
				<Text style={textColor('#555')}>TO CHANGE COLOR</Text>
			</View>

			<View style={footerStyle}>
				<Text style={textColor('#ffffff')}>{`+- POWERED BY REACT`}</Text>
			</View>
		</View>
	);
}

render(<App />);

waveforge.log("Counter demo ready");
