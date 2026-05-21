export type ColorLike = waveforge.ColorLike;
export type Style = Record<string, any>;

// ── Style combiner ──

export function style(...items: (Style | false | null | undefined)[]): Style {
	const out: Style = {};
	for (const item of items) {
		if (item) {
			Object.assign(out, item);
		}
	}
	return out;
}

// ── Box model shorthands ──

export function padding(v: number): Style;
export function padding(v: number, h: number): Style;
export function padding(t: number, r: number, b: number, l: number): Style;
export function padding(a: number, b?: number, c?: number, d?: number): Style {
	if (c !== undefined) {
		return { paddingTop: a, paddingRight: b!, paddingBottom: c, paddingLeft: d! };
	}
	if (b !== undefined) {
		return { paddingTop: a, paddingBottom: a, paddingRight: b, paddingLeft: b };
	}
	return { paddingTop: a, paddingRight: a, paddingBottom: a, paddingLeft: a };
}

export function margin(v: number): Style;
export function margin(v: number, h: number): Style;
export function margin(t: number, r: number, b: number, l: number): Style;
export function margin(a: number, b?: number, c?: number, d?: number): Style {
	if (c !== undefined) {
		return { marginTop: a, marginRight: b!, marginBottom: c, marginLeft: d! };
	}
	if (b !== undefined) {
		return { marginTop: a, marginBottom: a, marginRight: b, marginLeft: b };
	}
	return { marginTop: a, marginRight: a, marginBottom: a, marginLeft: a };
}

// ── Border shorthands ──

export function border(width: number, color?: ColorLike): Style {
	return {
		borderLeft: width,
		borderRight: width,
		borderTop: width,
		borderBottom: width,
		...(color
			? {
					borderLeftColor: color,
					borderRightColor: color,
					borderTopColor: color,
					borderBottomColor: color,
				}
			: {}),
	};
}

function borderEdge(edge: string, width: number, color?: ColorLike): Style {
	return {
		[`border${edge}`]: width,
		...(color ? { [`border${edge}Color`]: color } : {}),
	};
}
export const borderLeft = (w: number, c?: ColorLike): Style => borderEdge('Left', w, c);
export const borderRight = (w: number, c?: ColorLike): Style => borderEdge('Right', w, c);
export const borderTop = (w: number, c?: ColorLike): Style => borderEdge('Top', w, c);
export const borderBottom = (w: number, c?: ColorLike): Style => borderEdge('Bottom', w, c);

// ── Size & layout ──

export function size(w: number | string, h: number | string): Style {
	return { width: w, height: h };
}

export function fill(): Style {
	return { width: '100%', height: '100%' };
}

export function fullscreen(): Style {
	return { width: waveforge.width, height: waveforge.height };
}

export function flex(grow?: number, shrink?: number): Style {
	return {
		...(grow !== undefined ? { flexGrow: grow } : {}),
		...(shrink !== undefined ? { flexShrink: shrink } : {}),
	};
}

export function row(): Style {
	return { flexDirection: 'row' };
}

export function column(): Style {
	return { flexDirection: 'column' };
}

// ── Gap ──

export function gap(v: number | string): Style {
	return { gap: v };
}

export function rowGap(v: number | string): Style {
	return { rowGap: v };
}

export function columnGap(v: number | string): Style {
	return { columnGap: v };
}

// ── Alignment ──

export function center(): Style {
	return { justifyContent: 'center', alignItems: 'center' };
}

export function centerH(): Style {
	return { justifyContent: 'center' };
}

export function centerV(): Style {
	return { alignItems: 'center' };
}

// ── Visual ──

export function bg(color: ColorLike): Style {
	return { backgroundColor: color };
}

export function textColor(color: ColorLike): Style {
	return { color };
}

// ── Content alignment within layout node ──

export function contentAlignH(dir: 'left' | 'center' | 'right'): Style {
	return { contentAlignH: dir };
}

export function contentAlignV(dir: 'top' | 'horizon' | 'bottom'): Style {
	return { contentAlignV: dir };
}
