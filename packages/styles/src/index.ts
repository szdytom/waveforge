

// ── Style combiner ──

export function style(...items: (Record<string, any> | false | null | undefined)[]): Record<string, any> {
	const out: Record<string, any> = {};
	for (const item of items) {
		if (item) Object.assign(out, item);
	}
	return out;
}

// ── Box model shorthands ──

export function padding(v: number): Record<string, number>;
export function padding(v: number, h: number): Record<string, number>;
export function padding(t: number, r: number, b: number, l: number): Record<string, number>;
export function padding(a: number, b?: number, c?: number, d?: number): Record<string, number> {
	if (c !== undefined) return { paddingTop: a, paddingRight: b!, paddingBottom: c, paddingLeft: d! };
	if (b !== undefined) return { paddingTop: a, paddingBottom: a, paddingRight: b, paddingLeft: b };
	return { paddingTop: a, paddingRight: a, paddingBottom: a, paddingLeft: a };
}

export function margin(v: number): Record<string, number>;
export function margin(v: number, h: number): Record<string, number>;
export function margin(t: number, r: number, b: number, l: number): Record<string, number>;
export function margin(a: number, b?: number, c?: number, d?: number): Record<string, number> {
	if (c !== undefined) return { marginTop: a, marginRight: b!, marginBottom: c, marginLeft: d! };
	if (b !== undefined) return { marginTop: a, marginBottom: a, marginRight: b, marginLeft: b };
	return { marginTop: a, marginRight: a, marginBottom: a, marginLeft: a };
}

// ── Border shorthands ──

export function border(width: number, color?: string): Record<string, any> {
	return {
		borderLeft: width,
		borderRight: width,
		borderTop: width,
		borderBottom: width,
		...(color ? {
			borderLeftColor: color,
			borderRightColor: color,
			borderTopColor: color,
			borderBottomColor: color,
		} : {}),
	};
}

function borderEdge(edge: string, width: number, color?: string): Record<string, any> {
	return {
		[`border${edge}`]: width,
		...(color ? { [`border${edge}Color`]: color } : {}),
	};
}
export const borderLeft = (w: number, c?: string) => borderEdge('Left', w, c);
export const borderRight = (w: number, c?: string) => borderEdge('Right', w, c);
export const borderTop = (w: number, c?: string) => borderEdge('Top', w, c);
export const borderBottom = (w: number, c?: string) => borderEdge('Bottom', w, c);

// ── Size & layout ──

export function size(w: number, h: number): Record<string, number> {
	return { width: w, height: h };
}

export function flex(grow?: number, shrink?: number): Record<string, number> {
	return {
		...(grow !== undefined ? { flexGrow: grow } : {}),
		...(shrink !== undefined ? { flexShrink: shrink } : {}),
	};
}

export function row(): Record<string, string> {
	return { flexDirection: 'row' };
}

export function column(): Record<string, string> {
	return { flexDirection: 'column' };
}

// ── Alignment ──

export function center(): Record<string, string> {
	return { justifyContent: 'center', alignItems: 'center' };
}

export function centerH(): Record<string, string> {
	return { justifyContent: 'center' };
}

export function centerV(): Record<string, string> {
	return { alignItems: 'center' };
}

// ── Visual ──

export function bg(color: string): Record<string, string> {
	return { backgroundColor: color };
}

export function textColor(color: string): Record<string, string> {
	return { color };
}

// ── Content alignment within layout node ──

export function contentAlignH(dir: 'left' | 'center' | 'right'): Record<string, string> {
	return { contentAlignH: dir };
}

export function contentAlignV(dir: 'top' | 'horizon' | 'bottom'): Record<string, string> {
	return { contentAlignV: dir };
}
