import { Text, View } from '@waveforge/renderer';
import { center, column, fill, padding, row, style } from '@waveforge/styles';
import React from 'react';
import { UI_COLORS, UI_SPACING } from './theme.js';

export interface WidgetProps {
	children?: React.ReactNode;
	style?: Record<string, any>;
	[key: string]: any;
}

export interface ScreenProps extends WidgetProps {
	title?: string;
}

export function Screen({ title, children, style: extraStyle, ...props }: ScreenProps) {
	React.useEffect(() => {
		if (title) {
			waveforge.setTitle(title);
		}
	}, [title]);

	return (
		<View {...props} style={style(fill(), column(), { backgroundColor: UI_COLORS.background }, extraStyle)}>
			{children}
		</View>
	);
}

export function Row({ children, style: extraStyle, ...props }: WidgetProps) {
	return (
		<View {...props} style={style(row(), extraStyle)}>
			{children}
		</View>
	);
}

export function Column({ children, style: extraStyle, ...props }: WidgetProps) {
	return (
		<View {...props} style={style(column(), extraStyle)}>
			{children}
		</View>
	);
}

export function Center({ children, style: extraStyle, ...props }: WidgetProps) {
	return (
		<View {...props} style={style(center(), extraStyle)}>
			{children}
		</View>
	);
}

export function Panel({ children, style: extraStyle, ...props }: WidgetProps) {
	return (
		<View {...props} style={style(padding(UI_SPACING.md), { backgroundColor: UI_COLORS.panel }, extraStyle)}>
			{children}
		</View>
	);
}

export interface LabelProps extends Omit<WidgetProps, 'children'> {
	children: string;
	color?: waveforge.ColorLike;
	size?: number;
}

export function Label({ children, color = UI_COLORS.text, size = 1, style: extraStyle, ...props }: LabelProps) {
	return (
		<Text {...props} size={size} color={color} style={extraStyle}>
			{children}
		</Text>
	);
}

export function Title({ children, ...props }: LabelProps) {
	return (
		<Label size={2} {...props}>
			{children}
		</Label>
	);
}

export function Hint({ children, ...props }: LabelProps) {
	return (
		<Label color={UI_COLORS.mutedText} {...props}>
			{children}
		</Label>
	);
}
