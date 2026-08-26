import { type KeyboardEvent, Text, View } from '@waveforge/renderer';
import { contentAlignH, contentAlignV, padding, style } from '@waveforge/styles';
import React from 'react';
import { UI_COLORS } from './theme.js';

export type ButtonVariant = 'primary' | 'secondary' | 'accent';

const VARIANT_BG: Record<ButtonVariant, string> = {
	primary: UI_COLORS.primary,
	secondary: UI_COLORS.secondary,
	accent: UI_COLORS.accent,
};

const VARIANT_HOVER_BG: Record<ButtonVariant, string> = {
	primary: UI_COLORS.primaryHover,
	secondary: UI_COLORS.secondaryHover,
	accent: UI_COLORS.accentHover,
};

export interface ButtonProps {
	label: string;
	onClick?: () => void;
	variant?: ButtonVariant;
	style?: Record<string, any>;
	textSize?: number;
}

export function Button({ label, onClick, variant = 'primary', style: extra, textSize = 1 }: ButtonProps) {
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
			<Text size={textSize}>{label}</Text>
		</View>
	);
}
