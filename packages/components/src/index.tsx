import { type KeyboardEvent, Text, View } from '@waveforge/renderer';
import { contentAlignH, contentAlignV, padding, style } from '@waveforge/styles';
import React from 'react';

export type ButtonVariant = 'primary' | 'secondary' | 'accent';

const VARIANT_BG: Record<ButtonVariant, string> = {
	primary: '#4ecdc4',
	secondary: '#ff6b6b',
	accent: '#ffd93d',
};

const VARIANT_HOVER_BG: Record<ButtonVariant, string> = {
	primary: '#6edcd4',
	secondary: '#ff8b8b',
	accent: '#ffe36d',
};

export interface ButtonProps {
	label: string;
	onClick?: () => void;
	variant?: ButtonVariant;
	style?: Record<string, any>;
}

export function Button({ label, onClick, variant = 'primary', style: extra }: ButtonProps) {
	const [hovered, setHovered] = React.useState(false);
	const [focused, setFocused] = React.useState(false);
	const handleKeyDown = React.useCallback(
		(event: KeyboardEvent) => {
			if (event.code === 'Enter' || event.code === 'Space') {
				event.preventDefault();
				onClick?.();
			}
		},
		[onClick],
	);

	return (
		<View
			tabIndex={0}
			onClick={onClick}
			onPointerEnter={() => setHovered(true)}
			onPointerLeave={() => setHovered(false)}
			onFocus={() => setFocused(true)}
			onBlur={() => setFocused(false)}
			onKeyDown={handleKeyDown}
			style={style(
				padding(4, 14),
				contentAlignH('center'),
				contentAlignV('horizon'),
				{ backgroundColor: hovered || focused ? VARIANT_HOVER_BG[variant] : VARIANT_BG[variant] },
				extra,
			)}
		>
			<Text>{label}</Text>
		</View>
	);
}
