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
	position: 'position',
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

const EVENT_PROPS = new Set([
	'onClick',
	'onPointerDown',
	'onPointerUp',
	'onPointerEnter',
	'onPointerLeave',
	'onPointerMove',
	'onKeyDown',
	'onKeyUp',
	'onFocus',
	'onBlur',
]);

export type HostType = 'view' | 'text' | 'sprite';

export interface SyntheticEvent<NativeEvent = unknown> {
	readonly type: string;
	readonly target: waveforge.LayoutNode;
	currentTarget: waveforge.LayoutNode;
	readonly nativeEvent: NativeEvent;
	readonly propagationStopped: boolean;
	readonly defaultPrevented: boolean;
	stopPropagation(): void;
	preventDefault(): void;
}

export interface PointerEvent extends SyntheticEvent<waveforge.MouseButtonEvent | waveforge.MouseMoveEvent> {
	readonly x: number;
	readonly y: number;
	readonly button: number;
}

export interface KeyboardEvent extends SyntheticEvent<waveforge.KeyEvent> {
	readonly code: waveforge.KeyCode;
	readonly alt: boolean;
	readonly control: boolean;
	readonly shift: boolean;
	readonly system: boolean;
}

export interface FocusEvent extends SyntheticEvent<null> {
	readonly relatedTarget: waveforge.LayoutNode | null;
}

type EventHandler = (event: SyntheticEvent<any>) => void;

interface NodeMetadata {
	handlers: Record<string, EventHandler>;
	tabIndex: number | null;
}

const DEFAULT_TEXT_COLOR = '#f0e6ff';
const DEFAULT_LAYOUT_NODE = new waveforge.LayoutNode();

function metadata(node: waveforge.LayoutNode): NodeMetadata {
	const stored = (node as any)._wfMetadata as NodeMetadata | undefined;
	if (stored) {
		return stored;
	}
	const created = { handlers: {}, tabIndex: null };
	(node as any)._wfMetadata = created;
	return created;
}

function storeMetadata(node: waveforge.LayoutNode, props: Record<string, any> | null): void {
	const handlers: Record<string, EventHandler> = {};
	if (props) {
		for (const key of EVENT_PROPS) {
			if (typeof props[key] === 'function') {
				handlers[key] = props[key];
			}
		}
	}
	const tabIndex = typeof props?.tabIndex === 'number' ? props.tabIndex : null;
	(node as any)._wfMetadata = { handlers, tabIndex } satisfies NodeMetadata;
}

function effectiveLayoutProps(props: Record<string, any> | null): Record<string, any> {
	const result: Record<string, any> = {};
	if (!props) {
		return result;
	}
	for (const [key, value] of Object.entries(props.style || {})) {
		const propName = LAYOUT_PROP_MAP[key];
		if (propName) {
			result[propName] = value;
		}
	}
	for (const [key, value] of Object.entries(props)) {
		const propName = LAYOUT_PROP_MAP[key];
		if (propName) {
			result[propName] = value;
		}
	}
	return result;
}

function applyLayoutProps(
	node: waveforge.LayoutNode,
	previousProps: Record<string, any> | null,
	nextProps: Record<string, any> | null,
): void {
	const previous = effectiveLayoutProps(previousProps);
	const next = effectiveLayoutProps(nextProps);
	const propNames = new Set([...Object.keys(previous), ...Object.keys(next)]);

	for (const propName of propNames) {
		const nextValue = next[propName];
		if (nextValue === undefined || nextValue === null) {
			(node as any)[propName] = (DEFAULT_LAYOUT_NODE as any)[propName];
		} else if (!Object.is(previous[propName], nextValue)) {
			(node as any)[propName] = nextValue;
		}
	}
}

function setTextContent(node: waveforge.LayoutNode, text: string, size = 1, color?: waveforge.ColorLike): void {
	const textContent = new waveforge.TextContent(text, size);
	if (color) {
		textContent.color = color;
	}
	node.content = textContent;
}

function resolveTextColor(props: Record<string, any> | null): waveforge.ColorLike | undefined {
	return props?.color || props?.style?.color;
}

export function applyProps(
	node: waveforge.LayoutNode,
	type: HostType,
	previousProps: Record<string, any> | null,
	nextProps: Record<string, any> | null,
): void {
	applyLayoutProps(node, previousProps, nextProps);
	storeMetadata(node, nextProps);

	if (type === 'sprite') {
		const texture = nextProps?.texture;
		if (texture) {
			node.content = new waveforge.SpriteContent(texture, nextProps?.scale ?? 1);
		} else {
			node.content = null;
		}
	}

	if (type === 'text') {
		if (typeof nextProps?.children === 'string') {
			setTextContent(node, nextProps.children, nextProps.size ?? 1, resolveTextColor(nextProps) || DEFAULT_TEXT_COLOR);
		} else if (typeof previousProps?.children === 'string') {
			node.content = null;
		}
	}
}

export function createNode(type: HostType, props: Record<string, any> | null): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	applyProps(node, type, null, props);
	return node;
}

export function createTextNode(text: string): waveforge.LayoutNode {
	const node = new waveforge.LayoutNode();
	setTextContent(node, text, 1, DEFAULT_TEXT_COLOR);
	return node;
}

export function updateTextNode(node: waveforge.LayoutNode, text: string, size = 1, color?: waveforge.ColorLike): void {
	setTextContent(node, text, size, color || DEFAULT_TEXT_COLOR);
}

export function clearNode(node: waveforge.LayoutNode): void {
	while (node.firstChild) {
		node.removeChild(node.firstChild);
	}
}

function createSyntheticEvent<NativeEvent, Details extends object>(
	type: string,
	target: waveforge.LayoutNode,
	nativeEvent: NativeEvent,
	details: Details,
): SyntheticEvent<NativeEvent> & Details {
	let propagationStopped = false;
	let defaultPrevented = false;
	return Object.assign(
		{
			type,
			target,
			currentTarget: target,
			nativeEvent,
			get propagationStopped() {
				return propagationStopped;
			},
			get defaultPrevented() {
				return defaultPrevented;
			},
			stopPropagation() {
				propagationStopped = true;
			},
			preventDefault() {
				defaultPrevented = true;
			},
		},
		details,
	);
}

function dispatchBubbling(target: waveforge.LayoutNode, handlerName: string, event: SyntheticEvent): boolean {
	let handled = false;
	let node: waveforge.LayoutNode | undefined = target;
	while (node) {
		const handler = metadata(node).handlers[handlerName];
		if (handler) {
			event.currentTarget = node;
			handler(event);
			handled = true;
			if (event.propagationStopped) {
				break;
			}
		}
		node = node.parent;
	}
	return handled;
}

function buildPath(node: waveforge.LayoutNode | null): waveforge.LayoutNode[] {
	const path: waveforge.LayoutNode[] = [];
	let current: waveforge.LayoutNode | undefined = node || undefined;
	while (current) {
		path.push(current);
		current = current.parent;
	}
	path.reverse();
	return path;
}

function dispatchDirect(currentTarget: waveforge.LayoutNode, handlerName: string, event: SyntheticEvent): void {
	const handler = metadata(currentTarget).handlers[handlerName];
	if (handler) {
		event.currentTarget = currentTarget;
		handler(event);
	}
}

function pointerEvent(
	type: string,
	target: waveforge.LayoutNode,
	nativeEvent: waveforge.MouseButtonEvent | waveforge.MouseMoveEvent,
): PointerEvent {
	return createSyntheticEvent(type, target, nativeEvent, {
		x: nativeEvent.x,
		y: nativeEvent.y,
		button: 'button' in nativeEvent ? nativeEvent.button : -1,
	});
}

function keyboardEvent(target: waveforge.LayoutNode, nativeEvent: waveforge.KeyEvent): KeyboardEvent {
	return createSyntheticEvent(nativeEvent.type, target, nativeEvent, {
		code: nativeEvent.code,
		alt: nativeEvent.alt,
		control: nativeEvent.control,
		shift: nativeEvent.shift,
		system: nativeEvent.system,
	});
}

function focusEvent(
	type: 'focus' | 'blur',
	target: waveforge.LayoutNode,
	relatedTarget: waveforge.LayoutNode | null,
): FocusEvent {
	return createSyntheticEvent(type, target, null, { relatedTarget });
}

export class EventDispatcher {
	private hoveredNode: waveforge.LayoutNode | null = null;
	private focusedNode: waveforge.LayoutNode | null = null;
	private lastPointerMove: waveforge.MouseMoveEvent | null = null;

	constructor(private readonly root: waveforge.LayoutNode) {}

	dispatchPointerButton(nativeEvent: waveforge.MouseButtonEvent): boolean {
		const target = this.root.hitTest(nativeEvent.x, nativeEvent.y);
		if (!target) {
			if (nativeEvent.type === 'mousedown') {
				this.setFocus(null);
			}
			return false;
		}

		if (nativeEvent.type === 'mousedown') {
			const event = pointerEvent('pointerdown', target, nativeEvent);
			const handled = dispatchBubbling(target, 'onPointerDown', event);
			if (!event.defaultPrevented) {
				this.setFocus(this.closestFocusable(target));
			}
			return handled;
		}

		const pointerUpHandled = dispatchBubbling(target, 'onPointerUp', pointerEvent('pointerup', target, nativeEvent));
		const clickHandled = dispatchBubbling(target, 'onClick', pointerEvent('click', target, nativeEvent));
		return pointerUpHandled || clickHandled;
	}

	dispatchPointerMove(nativeEvent: waveforge.MouseMoveEvent): void {
		this.lastPointerMove = nativeEvent;
		const target = this.root.hitTest(nativeEvent.x, nativeEvent.y);
		if (target === this.hoveredNode) {
			if (target) {
				dispatchBubbling(target, 'onPointerMove', pointerEvent('pointermove', target, nativeEvent));
			}
			return;
		}

		const oldPath = buildPath(this.hoveredNode);
		const newPath = buildPath(target);
		let shared = 0;
		while (shared < oldPath.length && shared < newPath.length && oldPath[shared] === newPath[shared]) {
			shared++;
		}

		if (this.hoveredNode) {
			const event = pointerEvent('pointerleave', this.hoveredNode, nativeEvent);
			for (let index = oldPath.length - 1; index >= shared && !event.propagationStopped; index--) {
				dispatchDirect(oldPath[index], 'onPointerLeave', event);
			}
		}
		if (target) {
			const event = pointerEvent('pointerenter', target, nativeEvent);
			for (let index = shared; index < newPath.length && !event.propagationStopped; index++) {
				dispatchDirect(newPath[index], 'onPointerEnter', event);
			}
			dispatchBubbling(target, 'onPointerMove', pointerEvent('pointermove', target, nativeEvent));
		}
		this.hoveredNode = target;
	}

	dispatchPointerLeave(): void {
		if (this.hoveredNode && this.lastPointerMove) {
			const event = pointerEvent('pointerleave', this.hoveredNode, this.lastPointerMove);
			const path = buildPath(this.hoveredNode);
			for (let index = path.length - 1; index >= 0 && !event.propagationStopped; index--) {
				dispatchDirect(path[index], 'onPointerLeave', event);
			}
		}
		this.hoveredNode = null;
		this.lastPointerMove = null;
	}

	dispatchKey(nativeEvent: waveforge.KeyEvent): boolean {
		const target = this.focusedNode;
		if (!target) {
			if (nativeEvent.type === 'keydown' && nativeEvent.code === 'Tab') {
				this.focusNext(nativeEvent.shift);
				return true;
			}
			return false;
		}

		const event = keyboardEvent(target, nativeEvent);
		const handled = dispatchBubbling(target, nativeEvent.type === 'keydown' ? 'onKeyDown' : 'onKeyUp', event);
		if (nativeEvent.type === 'keydown' && nativeEvent.code === 'Tab' && !event.defaultPrevented) {
			this.focusNext(nativeEvent.shift);
			return true;
		}
		return handled;
	}

	focus(node: waveforge.LayoutNode): void {
		this.setFocus(this.closestFocusable(node));
	}

	blur(): void {
		this.setFocus(null);
	}

	validateNode(node: waveforge.LayoutNode): void {
		if (this.focusedNode === node && (metadata(node).tabIndex === null || node.display === 'none')) {
			this.setFocus(null);
		}
	}

	detach(node: waveforge.LayoutNode): void {
		if (this.focusedNode && this.isWithin(this.focusedNode, node)) {
			this.setFocus(null);
		}
		if (this.hoveredNode && this.isWithin(this.hoveredNode, node)) {
			this.hoveredNode = null;
			this.lastPointerMove = null;
		}
	}

	clear(): void {
		this.hoveredNode = null;
		this.focusedNode = null;
		this.lastPointerMove = null;
	}

	private closestFocusable(node: waveforge.LayoutNode): waveforge.LayoutNode | null {
		let current: waveforge.LayoutNode | undefined = node;
		while (current) {
			if (metadata(current).tabIndex !== null) {
				return current;
			}
			current = current.parent;
		}
		return null;
	}

	private isWithin(node: waveforge.LayoutNode, ancestor: waveforge.LayoutNode): boolean {
		let current: waveforge.LayoutNode | undefined = node;
		while (current) {
			if (current === ancestor) {
				return true;
			}
			current = current.parent;
		}
		return false;
	}

	private setFocus(next: waveforge.LayoutNode | null): void {
		if (next === this.focusedNode) {
			return;
		}
		const previous = this.focusedNode;
		this.focusedNode = next;
		if (previous) {
			dispatchBubbling(previous, 'onBlur', focusEvent('blur', previous, next));
		}
		if (next) {
			dispatchBubbling(next, 'onFocus', focusEvent('focus', next, previous));
		}
	}

	private focusNext(reverse: boolean): void {
		const focusable: waveforge.LayoutNode[] = [];
		this.collectFocusable(this.root, focusable);
		focusable.sort((left, right) => {
			const leftIndex = metadata(left).tabIndex || Number.MAX_SAFE_INTEGER;
			const rightIndex = metadata(right).tabIndex || Number.MAX_SAFE_INTEGER;
			return leftIndex - rightIndex;
		});
		if (focusable.length === 0) {
			this.setFocus(null);
			return;
		}

		const currentIndex = this.focusedNode ? focusable.indexOf(this.focusedNode) : -1;
		const offset = reverse ? -1 : 1;
		const start = currentIndex < 0 ? (reverse ? 0 : -1) : currentIndex;
		const nextIndex = (start + offset + focusable.length) % focusable.length;
		this.setFocus(focusable[nextIndex]);
	}

	private collectFocusable(node: waveforge.LayoutNode, result: waveforge.LayoutNode[]): void {
		const tabIndex = metadata(node).tabIndex;
		if (tabIndex !== null && tabIndex >= 0 && node.display !== 'none') {
			result.push(node);
		}
		for (let index = 0; index < node.childCount; index++) {
			const child = node.childItem(index);
			if (child) {
				this.collectFocusable(child, result);
			}
		}
	}
}
