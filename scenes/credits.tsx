import { Column, Label, playUiSound, Row, Screen, Title, useKeyDown } from '@waveforge/components';
import { render } from '@waveforge/renderer';
import React from 'react';

const CREDITS = [
	['Code', 'fang_erj'],
	['Music and SFX', 'stevvven'],
	['Pixel Art', 'ZTL-UwU'],
] as const;

function CreditsScene() {
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
		<Screen title="Credits" onClick={goBack}>
			<Title style={{ position: 'absolute', left: 16, top: 14 }}>Credits</Title>
			<Column style={{ position: 'absolute', left: 32, top: 47, width: 192, gap: 6 }}>
				{CREDITS.map(([label, value]) => (
					<Row key={label} style={{ justifyContent: 'spaceBetween', height: 6 }}>
						<Label>{label}</Label>
						<Label>{value}</Label>
					</Row>
				))}
			</Column>
		</Screen>
	);
}

render(<CreditsScene />);
