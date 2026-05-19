import { Text, View } from '@waveforge/renderer';
import { contentAlignH, contentAlignV, padding, style } from '@waveforge/styles';

export type ButtonVariant = 'primary' | 'secondary' | 'accent';

const VARIANT_BG: Record<ButtonVariant, string> = {
	primary: '#4ecdc4',
	secondary: '#ff6b6b',
	accent: '#ffd93d',
};

export interface ButtonProps {
	label: string;
	onClick?: () => void;
	variant?: ButtonVariant;
	style?: Record<string, any>;
}

export function Button({ label, onClick, variant = 'primary', style: extra }: ButtonProps) {
	return (
		<View
			onClick={onClick}
			style={style(
				padding(4, 14),
				contentAlignH('center'),
				contentAlignV('horizon'),
				{ backgroundColor: VARIANT_BG[variant] },
				extra,
			)}
		>
			<Text>{label}</Text>
		</View>
	);
}
