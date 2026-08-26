import { playUiSound, Screen, useKeyDown } from '@waveforge/components';
import { render, Sprite } from '@waveforge/renderer';
import { fill } from '@waveforge/styles';
import React from 'react';

const helpTexture = new waveforge.Texture('ui/help');

function HelpScene() {
	const goBack = React.useCallback(() => {
		playUiSound('forward');
		waveforge.navigateTo('main-menu');
	}, []);

	useKeyDown((event) => {
		if (event.code === 'Escape' || event.code === 'Enter' || event.code === 'Space') {
			goBack();
		}
	});

	return (
		<Screen title="Help" onClick={goBack}>
			<Sprite texture={helpTexture} style={fill()} />
		</Screen>
	);
}

render(<HelpScene />);
