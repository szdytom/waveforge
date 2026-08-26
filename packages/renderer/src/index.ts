import Reconciler from 'react-reconciler';
import { DefaultEventPriority } from 'react-reconciler/constants';
import {
	applyProps,
	clearNode,
	createNode,
	createTextNode,
	EventDispatcher,
	type HostType,
	updateTextNode,
} from './host-config.js';

export type { FocusEvent, KeyboardEvent, PointerEvent, SyntheticEvent } from './host-config.js';

// ── Priority tracking (needed by reconciler internally) ──
let _currentUpdatePriority = 0;

// ── Host Config ──
const hostConfig = {
	supportsMutation: true,
	supportsPersistence: false,
	supportsHydration: false,
	supportsResources: false,
	supportsSingletons: false,
	supportsTestSelectors: false,

	rendererVersion: '19.0.0',
	rendererPackageName: '@waveforge/renderer',

	shouldAttemptEagerTransition(): boolean {
		return false;
	},

	getCurrentUpdatePriority(): number {
		return _currentUpdatePriority;
	},

	setCurrentUpdatePriority(newPriority: number): void {
		_currentUpdatePriority = newPriority;
	},

	resolveUpdatePriority(): number {
		if (_currentUpdatePriority !== 0) {
			return _currentUpdatePriority;
		}
		return DefaultEventPriority;
	},

	createInstance(
		type: HostType,
		props: Record<string, any> | null,
		_rootContainer: any,
		_hostContext: any,
		_internalHandle: any,
	): any {
		return createNode(type, props);
	},

	createTextInstance(text: string, _rootContainer: any, _hostContext: any, _internalHandle: any): any {
		return createTextNode(text);
	},

	appendInitialChild(parent: any, child: any): void {
		parent.appendChild(child);
	},

	finalizeInitialChildren(
		_instance: any,
		_type: HostType,
		_props: any,
		_rootContainer: any,
		_hostContext: any,
	): boolean {
		return false;
	},

	shouldSetTextContent(type: HostType, props: Record<string, any>): boolean {
		return type === 'text' && typeof props.children === 'string';
	},

	getRootHostContext(_rootContainer: any): any {
		return null;
	},

	getChildHostContext(parentHostContext: any, _type: string, _rootContainer: any): any {
		return parentHostContext;
	},

	getPublicInstance(instance: any): any {
		return instance;
	},

	prepareForCommit(_container: any): any {
		return null;
	},

	resetAfterCommit(_container: any): void {},

	preparePortalMount(_containerInfo: any): void {},

	scheduleTimeout: (fn: () => void, delay: number) => setTimeout(fn, delay ?? 0),
	cancelTimeout: (id: any) => clearTimeout(id),
	noTimeout: -1,

	supportsMicrotasks: true,
	scheduleMicrotask: (fn: () => void) => queueMicrotask(fn),

	isPrimaryRenderer: true,
	getCurrentEventPriority(): number {
		return DefaultEventPriority;
	},

	now: (): number => performance.now(),

	// ── Mutation Methods ──

	appendChild(parent: any, child: any): void {
		parent.appendChild(child);
	},

	appendChildToContainer(container: any, child: any): void {
		container.appendChild(child);
	},

	insertBefore(parent: any, child: any, beforeChild: any): void {
		parent.insertBefore(child, beforeChild);
	},

	insertInContainerBefore(container: any, child: any, beforeChild: any): void {
		container.insertBefore(child, beforeChild);
	},

	removeChild(parent: any, child: any): void {
		parent.removeChild(child);
	},

	removeChildFromContainer(container: any, child: any): void {
		container.removeChild(child);
	},

	resetTextContent(instance: any): void {
		instance.content = null;
	},

	commitTextUpdate(textInstance: any, _prevText: string, nextText: string): void {
		const existing = textInstance.content;
		const color = existing instanceof waveforge.TextContent ? existing.color?.toString() : undefined;
		const size = existing instanceof waveforge.TextContent ? existing.size : 1;
		updateTextNode(textInstance, nextText, size, color);
	},

	commitMount(_instance: any, _type: string, _props: any, _internalHandle: any): void {},

	commitUpdate(instance: any, _type: HostType, _prevProps: any, nextProps: any, _internalHandle: any): void {
		applyProps(instance, _type, _prevProps, nextProps);
		_eventDispatcher?.validateNode(instance);
	},

	hideInstance(instance: any): void {
		instance.display = 'none';
	},

	hideTextInstance(textInstance: any): void {
		textInstance.display = 'none';
	},

	unhideInstance(instance: any, props: any): void {
		instance.display = props?.style?.display || 'flex';
	},

	unhideTextInstance(textInstance: any, _text: string): void {
		textInstance.display = 'flex';
	},

	clearContainer(container: any): void {
		clearNode(container);
	},

	maySuspendCommit(_type: HostType, _props: any): boolean {
		return false;
	},

	detachDeletedInstance(instance: any): void {
		_eventDispatcher?.detach(instance);
	},

	resetFormInstance(): void {},
};

// ── Reconciler Instance ──

const reconciler = Reconciler(hostConfig as any);

// ── Root tracking & event dispatch ──

let _currentRoot: waveforge.LayoutNode | null = null;
let _currentContainer: any = null;
let _eventDispatcher: EventDispatcher | null = null;
let _setup = false;

function unmountCurrentRoot(): void {
	if (!_currentContainer) {
		return;
	}
	const activeContainer = _currentContainer;
	_currentContainer = null;
	_currentRoot = null;
	_eventDispatcher?.clear();
	_eventDispatcher = null;
	(reconciler as any).updateContainerSync(null, activeContainer, null, null);
	(reconciler as any).flushSyncWork();
	(reconciler as any).flushPassiveEffects();
}

function setupEngine(): void {
	if (_setup) {
		return;
	}
	_setup = true;

	waveforge.addEventListener('step', () => {
		if (_currentRoot) {
			waveforge.commitLayout(_currentRoot);
		}
	});

	waveforge.addEventListener('mousebutton', (event) => {
		_eventDispatcher?.dispatchPointerButton(event);
	});

	waveforge.addEventListener('mousemove', (event) => {
		_eventDispatcher?.dispatchPointerMove(event);
	});

	waveforge.addEventListener('mouseleave', () => {
		_eventDispatcher?.dispatchPointerLeave();
	});

	waveforge.addEventListener('key', (event) => {
		_eventDispatcher?.dispatchKey(event);
	});

	waveforge.addEventListener('teardown', () => {
		unmountCurrentRoot();
	});
}

// ── Public API ──

export function render(element: any, rootNode?: waveforge.LayoutNode): any {
	unmountCurrentRoot();
	if (!rootNode) {
		rootNode = new waveforge.LayoutNode();
		rootNode.width = waveforge.width;
		rootNode.height = waveforge.height;
	}
	_currentRoot = rootNode;
	_eventDispatcher = new EventDispatcher(rootNode);
	setupEngine();

	const container = reconciler.createContainer(
		rootNode,
		0, // tag: LegacyRoot
		null, // hydrationCallbacks
		false, // isStrictMode
		null, // concurrentUpdatesByDefault
		'', // identifierPrefix
		console.error,
		null, // transitionCallbacks
	);

	reconciler.updateContainer(element, container, null, null);
	_currentContainer = container;

	return container;
}

export function update(container: any, element: any): void {
	reconciler.updateContainer(element, container, null, null);
}

export function unmount(container?: any): void {
	if (!container || container === _currentContainer) {
		unmountCurrentRoot();
		return;
	}
	(reconciler as any).updateContainerSync(null, container, null, null);
	(reconciler as any).flushSyncWork();
	(reconciler as any).flushPassiveEffects();
}

export function focus(node: waveforge.LayoutNode): void {
	_eventDispatcher?.focus(node);
}

export function blur(): void {
	_eventDispatcher?.blur();
}

// Host component type constants
export const View = 'view' as unknown as React.ComponentType<any>;
export const Text = 'text' as unknown as React.ComponentType<any>;
export const Sprite = 'sprite' as unknown as React.ComponentType<any>;
