import React from 'react';
import { render, View, Text } from '@waveforge/renderer';
import { Button } from '@waveforge/components';

waveforge.log("Counter demo loading...");

const BG_COLORS = ['#f0e6ff', '#e6f5ff', '#fff0e6', '#e6ffe6', '#fffae6'];

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
		<View style={{
			width: 256,
			height: 192,
			flexDirection: 'column',
			backgroundColor: BG_COLORS[bgIndex],
		}}>
			{/* Header */}
			<View style={{
				height: 18,
				backgroundColor: '#2d2d5e',
				flexDirection: 'row',
				alignItems: 'center',
				justifyContent: 'center',
				borderBottom: 1,
				borderBottomColor: '#4ecdc455',
			}}>
				<Text style={{ color: '#ffffff' }}>{`REACT COUNTER DEMO`}</Text>
			</View>

			{/* Counter display */}
			<View style={{
				flexGrow: 1,
				flexDirection: 'column',
				justifyContent: 'center',
				alignItems: 'center',
			}}>
				<View style={{
					backgroundColor: '#ffffffcc',
					paddingTop: 6,
					paddingRight: 24,
					paddingBottom: 6,
					paddingLeft: 24,
					borderLeft: 2,
					borderLeftColor: '#4ecdc4',
					borderRight: 2,
					borderRightColor: '#4ecdc4',
					marginBottom: 10,
				}}>
					<Text style={{ color: '#2d2d5e' }}>{`COUNT: ${count}`}</Text>
				</View>

				{/* Button row */}
				<View style={{
					flexDirection: 'row',
					alignItems: 'center',
				}}>
					<Button
						label={"+1"}
						variant="primary"
						onClick={() => increment(1)}
						style={{ marginRight: 4, marginLeft: 4 }}
					/>
					<Button
						label={"+5"}
						variant="accent"
						onClick={() => increment(5)}
						style={{ marginRight: 4, marginLeft: 4 }}
					/>
					<Button
						label={"RESET"}
						variant="secondary"
						onClick={reset}
						style={{ marginRight: 4, marginLeft: 4 }}
					/>
				</View>

				<View style={{ height: 6 }} />

				<Text style={{ color: '#555' }}>{`CLICK +1 OR +5`}</Text>
				<Text style={{ color: '#555' }}>{`TO CHANGE COLOR`}</Text>
			</View>

			{/* Footer */}
			<View style={{
				height: 12,
				flexDirection: 'row',
				alignItems: 'center',
				justifyContent: 'center',
				backgroundColor: '#2d2d5e',
				borderTop: 1,
				borderTopColor: '#4ecdc455',
			}}>
				<Text style={{ color: '#ffffff' }}>{`+- POWERED BY REACT`}</Text>
			</View>
		</View>
	);
}

const rootNode = new waveforge.LayoutNode();
rootNode.width = 256;
rootNode.height = 192;

render(<App />, rootNode);

waveforge.log("Counter demo ready");
