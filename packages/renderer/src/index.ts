import '@waveforge/timer';

import Reconciler from 'react-reconciler';
import { DefaultEventPriority } from 'react-reconciler/constants';
import { applyProps, createNode, createTextNode, clearNode, updateTextNode, dispatchClick, HostType } from './host-config.js';

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

	createTextInstance(
		text: string,
		_rootContainer: any,
		_hostContext: any,
		_internalHandle: any,
	): any {
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

	resetAfterCommit(_container: any): void {
	},

	preparePortalMount(_containerInfo: any): void {
	},

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
		const color = existing instanceof waveforge.TextContent
			? existing.color?.toString()
			: undefined;
		updateTextNode(textInstance, nextText, color);
	},

	commitMount(_instance: any, _type: string, _props: any, _internalHandle: any): void {
	},

	commitUpdate(
		instance: any,
		_type: HostType,
		_prevProps: any,
		nextProps: any,
		_internalHandle: any,
	): void {
		applyProps(instance, _type, nextProps);
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

	detachDeletedInstance(): void {
	},

	resetFormInstance(): void {
	},
};

// ── Reconciler Instance ──

const reconciler = Reconciler(hostConfig as any);

// ── Root tracking & event dispatch ──

let _currentRoot: waveforge.LayoutNode | null = null;
let _setup = false;

function setupEngine(): void {
	if (_setup) return;
	_setup = true;

	waveforge.addEventListener('step', () => {
		if (_currentRoot) {
			waveforge.commitLayout(_currentRoot);
		}
	});

	waveforge.addEventListener('mousebutton', (event) => {
		if (event.type !== 'mouseup' || !_currentRoot) return;
		dispatchClick(_currentRoot, event.x, event.y);
	});
}

// ── Public API ──

export function render(element: any, rootNode: waveforge.LayoutNode): any {
	_currentRoot = rootNode;
	setupEngine();

	const container = reconciler.createContainer(
		rootNode,
		0,    // tag: LegacyRoot
		null, // hydrationCallbacks
		false,// isStrictMode
		null, // concurrentUpdatesByDefault
		'',   // identifierPrefix
		console.error,
		null, // transitionCallbacks
	);

	reconciler.updateContainer(element, container, null, null);

	return container;
}

export function update(container: any, element: any): void {
	reconciler.updateContainer(element, container, null, null);
}

// Host component type constants
export const View = 'view' as unknown as React.ComponentType<any>;
export const Text = 'text' as unknown as React.ComponentType<any>;
export const Sprite = 'sprite' as unknown as React.ComponentType<any>;
