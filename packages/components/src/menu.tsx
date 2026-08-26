import { Text, View } from '@waveforge/renderer';
import { contentAlignV, padding, style } from '@waveforge/styles';
import { UI_COLORS } from './theme.js';

export interface MenuItemProps {
	label: string;
	selected?: boolean;
	disabled?: boolean;
	onSelect?(): void;
	onActivate?(): void;
	style?: Record<string, any>;
	value?: string;
}

export function MenuItem({
	label,
	selected = false,
	disabled = false,
	onSelect,
	onActivate,
	style: extraStyle,
	value,
}: MenuItemProps) {
	const color = disabled ? UI_COLORS.mutedText : selected ? UI_COLORS.active : UI_COLORS.text;
	return (
		<View
			tabIndex={disabled ? undefined : -1}
			onPointerEnter={disabled ? undefined : onSelect}
			onClick={disabled ? undefined : onActivate}
			style={style(padding(2, 4), contentAlignV('horizon'), extraStyle)}
		>
			<Text color={color}>{label}</Text>
			{value ? (
				<Text color={color} style={{ marginLeft: 'auto' }}>
					{selected ? `<${value}>` : value}
				</Text>
			) : null}
		</View>
	);
}
