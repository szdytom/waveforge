import { Column, Hint, Label, playUiSound, Row, Screen, Title, useKeyDown } from '@waveforge/components';
import { render } from '@waveforge/renderer';
import { padding, style } from '@waveforge/styles';
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
		<Screen title="Credits" onClick={goBack} style={padding(16)}>
			<Title>Credits</Title>
			<Column style={style({ width: 192, marginLeft: 16, marginTop: 18 }, { gap: 6 })}>
				{CREDITS.map(([label, value]) => (
					<Row key={label} style={{ justifyContent: 'spaceBetween' }}>
						<Label>{label}</Label>
						<Label>{value}</Label>
					</Row>
				))}
			</Column>
			<Hint style={{ marginTop: 'auto' }}>Press Enter or Escape to return</Hint>
		</Screen>
	);
}

render(<CreditsScene />);
