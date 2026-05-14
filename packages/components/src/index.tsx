import React from 'react';
import { View, Text } from '@waveforge/renderer';

export type ButtonVariant = 'primary' | 'secondary' | 'accent';

const VARIANT_STYLES: Record<ButtonVariant, Record<string, any>> = {
	primary: {
		backgroundColor: '#4ecdc4',
	},
	secondary: {
		backgroundColor: '#ff6b6b',
	},
	accent: {
		backgroundColor: '#ffd93d',
	},
};

export interface ButtonProps {
	label: string;
	onClick?: () => void;
	variant?: ButtonVariant;
	style?: Record<string, any>;
}

export function Button({ label, onClick, variant = 'primary', style }: ButtonProps) {
	const merged = {
		...VARIANT_STYLES[variant],
		...style,
	};
	return (
		<View
			onClick={onClick}
			style={{
				paddingTop: 4,
				paddingRight: 14,
				paddingBottom: 4,
				paddingLeft: 14,
				contentAlignH: 'center',
				contentAlignV: 'horizon',
				...merged,
			}}
		>
			<Text>{label}</Text>
		</View>
	);
}
