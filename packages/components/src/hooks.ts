import React from 'react';

interface EngineEventMap {
	step: undefined;
	key: waveforge.KeyEvent;
	mousebutton: waveforge.MouseButtonEvent;
	mousemove: waveforge.MouseMoveEvent;
	mouseenter: undefined;
	mouseleave: undefined;
	teardown: undefined;
}

type EngineEventHandler<EventType extends keyof EngineEventMap> = EngineEventMap[EventType] extends undefined
	? () => void
	: (event: EngineEventMap[EventType]) => void;

export function useEngineEvent<EventType extends keyof EngineEventMap>(
	type: EventType,
	handler: EngineEventHandler<EventType>,
): void {
	const handlerRef = React.useRef(handler);
	handlerRef.current = handler;

	React.useEffect(() => {
		const listener = (...args: unknown[]) => (handlerRef.current as (...values: unknown[]) => void)(...args);
		(waveforge.addEventListener as any)(type, listener);
		return () => waveforge.removeEventListener(type, listener);
	}, [type]);
}

export function useStep(handler: () => void): void {
	useEngineEvent('step', handler);
}

export function useKeyDown(handler: (event: waveforge.KeyEvent) => void): void {
	useEngineEvent('key', (event) => {
		if (event.type === 'keydown') {
			handler(event);
		}
	});
}

let forwardSound: waveforge.Sound | null = null;
let backwardSound: waveforge.Sound | null = null;

export function playUiSound(direction: 'forward' | 'backward'): void {
	if (direction === 'forward') {
		forwardSound ??= new waveforge.Sound('sfx/forward');
		forwardSound.play();
		return;
	}
	backwardSound ??= new waveforge.Sound('sfx/backward');
	backwardSound.play();
}

export interface MenuNavigationOptions {
	itemCount: number;
	onActivate(index: number): void;
	onBack?(): void;
	orientation?: 'horizontal' | 'vertical';
	initialIndex?: number;
	wrap?: boolean;
}

export interface MenuNavigation {
	selectedIndex: number;
	setSelectedIndex: React.Dispatch<React.SetStateAction<number>>;
}

export function useMenuNavigation({
	itemCount,
	onActivate,
	onBack,
	orientation = 'vertical',
	initialIndex = 0,
	wrap = true,
}: MenuNavigationOptions): MenuNavigation {
	const [selectedIndex, setSelectedIndex] = React.useState(initialIndex);

	React.useEffect(() => {
		setSelectedIndex((current) => Math.max(0, Math.min(current, itemCount - 1)));
	}, [itemCount]);

	useKeyDown((event) => {
		const previousKeys = orientation === 'vertical' ? ['ArrowUp', 'w'] : ['ArrowLeft', 'a'];
		const nextKeys = orientation === 'vertical' ? ['ArrowDown', 's'] : ['ArrowRight', 'd'];
		if (previousKeys.includes(event.code)) {
			playUiSound('backward');
			setSelectedIndex((current) => (current > 0 ? current - 1 : wrap ? itemCount - 1 : current));
		} else if (nextKeys.includes(event.code)) {
			playUiSound('forward');
			setSelectedIndex((current) => (current + 1 < itemCount ? current + 1 : wrap ? 0 : current));
		} else if (event.code === 'Enter' || event.code === 'Space') {
			onActivate(selectedIndex);
		} else if (event.code === 'Escape') {
			onBack?.();
		}
	});

	return { selectedIndex, setSelectedIndex };
}
