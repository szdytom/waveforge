const LAYOUT_PROP_MAP: Record<string, string> = {
	width: 'width',
	height: 'height',
	minWidth: 'minWidth',
	maxWidth: 'maxWidth',
	minHeight: 'minHeight',
	maxHeight: 'maxHeight',

	flexDirection: 'flexDirection',
	justifyContent: 'justifyContent',
	alignItems: 'alignItems',
	alignSelf: 'alignSelf',
	alignContent: 'alignContent',
	flexWrap: 'flexWrap',
	overflow: 'overflow',
	display: 'display',
	positionType: 'positionType',
	flex: 'flex',
	flexGrow: 'flexGrow',
	flexShrink: 'flexShrink',

	direction: 'direction',

	contentAlignH: 'contentAlignH',
	contentAlignV: 'contentAlignV',

	marginLeft: 'marginLeft',
	marginRight: 'marginRight',
	marginTop: 'marginTop',
	marginBottom: 'marginBottom',
	paddingLeft: 'paddingLeft',
	paddingRight: 'paddingRight',
	paddingTop: 'paddingTop',
	paddingBottom: 'paddingBottom',

	gap: 'gap',
	rowGap: 'rowGap',
	columnGap: 'columnGap',

	left: 'left',
	right: 'right',
	top: 'top',
	bottom: 'bottom',

	borderLeft: 'borderLeft',
	borderRight: 'borderRight',
	borderTop: 'borderTop',
	borderBottom: 'borderBottom',

	borderLeftColor: 'borderLeftColor',
	borderRightColor: 'borderRightColor',
	borderTopColor: 'borderTopColor',
	borderBottomColor: 'borderBottomColor',

	backgroundColor: 'backgroundColor',
};

const SKIP_PROPS = new Set(['style', 'children', 'key', 'ref']);
const EVENT_PROPS = new Set(['onClick', 'onPointerEnter', 'onPointerLeave', 'onPointerMove']);

export type HostType = 'view' | 'text' | 'sprite';

const DEFAULT_TEXT_COLOR = '#f0e6ff';

function storeEventHandlers(node: waveforge.LayoutNode, props: Record<string, any> | null): void {
	const hd: Record<string, Function> = {};
	if (props) {
		for (const key of EVENT_PROPS) {
			if (typeof props[key] === 'function') {
				hd[key] = props[key];
			}
		}
	}
	(node as any)._wfEv = hd;
}

function setTextContent(node: waveforge.LayoutNode, text: string, color?: string): void {
	const tc = new waveforge.TextContent(text, 1);
	if (color) {
		tc.color = color;
	}
	node.content = tc;
}

function resolveTextColor(props: Record<string, any> | null): string | undefined {
	if (!props) {
		return undefined;
	}
	const fromStyle = props.style?.color;
	if (fromStyle) {
		return fromStyle;
	}
	return props.color;
}

export function applyProps(node: waveforge.LayoutNode, type: HostType, props: Record<string, any> | null): void {
	if (!props) {
		return;
	}

	const style: Record<string, any> = props.style || {};
	for (const [key, value] of Object.entries(style)) {
		const propName = LAYOUT_PROP_MAP[key];
		if (propName && value !== undefined && value !== null) {
			(node as any)[propName] = value;
		}
	}

	for (const [key, value] of Object.entries(props)) {
		if (SKIP_PROPS.has(key) || key === 'style') {
			continue;
		}
		if (EVENT_PROPS.has(key)) {
			continue;
		}
		const propName = LAYOUT_PROP_MAP[key];
		if (propName && value !== undefined && value !== null) {
			(node as any)[propName] = value;
		}
	}

	storeEventHandlers(node, props);

	if (type === 'text' && typeof props.children === 'string') {
		const color = resolveTextColor(props) || DEFAULT_TEXT_COLOR;
		setTextContent(node, props.children, color);
	}
}

export function createNode(type: HostType, props: Record<string, any> | null): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	applyProps(node, type, props);
	return node;
}

export function createTextNode(text: string): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	setTextContent(node, text, DEFAULT_TEXT_COLOR);
	return node;
}

export function updateTextNode(node: waveforge.LayoutNode, text: string, color?: string): void {
	setTextContent(node, text, color || DEFAULT_TEXT_COLOR);
}

export function clearNode(node: waveforge.LayoutNode): void {
	while (node.firstChild) {
		node.removeChild(node.firstChild);
	}
}

export function dispatchClick(root: waveforge.LayoutNode, x: number, y: number): boolean {
	const target = root.hitTest(x, y);
	if (!target) {
		return false;
	}

	let node: any = target;
	while (node) {
		const handlers = node._wfEv as Record<string, Function> | undefined;
		if (handlers?.onClick) {
			handlers.onClick();
			return true;
		}
		node = node.parent;
	}
	return false;
}

// ── Hover state tracking ──

let _hoveredNode: waveforge.LayoutNode | null = null;

function walkChain(node: any, eventName: string): void {
	while (node) {
		const handlers = node._wfEv as Record<string, Function> | undefined;
		if (handlers?.[eventName]) {
			handlers[eventName]();
		}
		node = node.parent;
	}
}

function buildPath(node: any): any[] {
	const path: any[] = [];
	while (node) {
		path.push(node);
		node = node.parent;
	}
	path.reverse();
	return path;
}

function leaveHoveredChain(): void {
	if (_hoveredNode) {
		walkChain(_hoveredNode, 'onPointerLeave');
		_hoveredNode = null;
	}
}

export function dispatchHoverChange(root: waveforge.LayoutNode, x: number, y: number): void {
	const target = root.hitTest(x, y);
	if (!target) {
		leaveHoveredChain();
		return;
	}

	if (target === _hoveredNode) {
		walkChain(target, 'onPointerMove');
		return;
	}

	if (_hoveredNode) {
		const oldPath = buildPath(_hoveredNode);
		const newPath = buildPath(target);

		let lca = 0;
		while (lca < oldPath.length && lca < newPath.length && oldPath[lca] === newPath[lca]) {
			lca++;
		}

		for (let i = oldPath.length - 1; i >= lca; i--) {
			const handlers = oldPath[i]._wfEv as Record<string, Function> | undefined;
			if (handlers?.onPointerLeave) {
				handlers.onPointerLeave();
			}
		}

		for (let i = lca; i < newPath.length; i++) {
			const handlers = newPath[i]._wfEv as Record<string, Function> | undefined;
			if (handlers?.onPointerEnter) {
				handlers.onPointerEnter();
			}
		}
	} else {
		walkChain(target, 'onPointerEnter');
	}

	_hoveredNode = target;
}

export function dispatchHoverLeave(): void {
	leaveHoveredChain();
}
