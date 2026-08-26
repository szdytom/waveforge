import { Text, View } from '@waveforge/renderer';
import { contentAlignV, padding, row, style } from '@waveforge/styles';
import { UI_COLORS } from './theme.js';

export interface MenuItemProps {
	label: string;
	selected?: boolean;
	disabled?: boolean;
	onSelect?(): void;
	onActivate?(): void;
	style?: Record<string, any>;
	value?: string;
	textSize?: number;
	color?: waveforge.ColorLike;
	selectedColor?: waveforge.ColorLike;
}

export function MenuItem({
	label,
	selected = false,
	disabled = false,
	onSelect,
	onActivate,
	style: extraStyle,
	value,
	textSize = 1,
	color = UI_COLORS.text,
	selectedColor = UI_COLORS.active,
}: MenuItemProps) {
	const textColor = disabled ? UI_COLORS.mutedText : selected ? selectedColor : color;
	return (
		<View
			tabIndex={disabled ? undefined : -1}
			onPointerEnter={disabled ? undefined : onSelect}
			onClick={disabled ? undefined : onActivate}
			style={style(
				row(),
				padding(2, 4),
				contentAlignV('horizon'),
				value ? { justifyContent: 'spaceBetween' } : null,
				extraStyle,
			)}
		>
			<Text color={textColor} size={textSize}>
				{label}
			</Text>
			{value ? (
				<Text color={textColor} size={textSize}>
					{selected ? `<${value}>` : `${value} `}
				</Text>
			) : null}
		</View>
	);
}
