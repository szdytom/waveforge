import { Label, useStep } from '@waveforge/components';
import { render, View } from '@waveforge/renderer';
import { center, fill } from '@waveforge/styles';
import React from 'react';

function LoadingScene() {
	const levelId = waveforge.routeData;
	if (!levelId) {
		throw new Error('Loading scene requires a level id');
	}

	const tickRef = React.useRef(0);
	React.useEffect(() => {
		waveforge.music.fadeOut(waveforge.transitions.fadeOutTicks);
	}, []);
	useStep(() => {
		tickRef.current += 1;
		if (tickRef.current >= waveforge.transitions.fadeOutTicks) {
			waveforge.navigateTo('play-level', levelId);
		}
	});

	return (
		<View style={{ ...fill(), ...center(), backgroundColor: '#ffffff' }}>
			<Label>LOADING...</Label>
		</View>
	);
}

render(<LoadingScene />);
