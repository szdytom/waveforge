/// <reference path="./waveforge.d.ts" />

interface TimerEntry {
	id: number;
	callback: () => void;
	fireAt: number;
	isInterval: boolean;
	intervalMs: number;
}

let queue: (TimerEntry | null)[] = [];
let nextId = 1;
let compactAt = performance.now() + 60000;

function checkTimers(): void {
	const now = performance.now();
	const due: TimerEntry[] = [];

	for (let i = 0; i < queue.length; i++) {
		const t = queue[i];
		if (t === null) continue;
		if (now >= t.fireAt) {
			due.push(t);
			queue[i] = null;
		}
	}

	for (const t of due) {
		try { t.callback(); } catch (_) { /* don't let one bad timer break the loop */ }
	}

	for (const t of due) {
		if (t.isInterval) {
			queue.push({
				id: t.id,
				callback: t.callback,
				fireAt: now + t.intervalMs,
				isInterval: true,
				intervalMs: t.intervalMs,
			});
		}
	}

	if (now > compactAt) {
		queue = queue.filter(t => t !== null);
		compactAt = now + 60000;
	}
}

export function setTimeout(callback: () => void, delayMs = 0): number {
	const id = nextId++;
	queue.push({
		id,
		callback,
		fireAt: performance.now() + Math.max(0, delayMs),
		isInterval: false,
		intervalMs: 0,
	});
	return id;
}

export function clearTimeout(id: number): void {
	for (let i = 0; i < queue.length; i++) {
		if (queue[i]?.id === id) {
			queue[i] = null;
			break;
		}
	}
}

export function setInterval(callback: () => void, intervalMs = 0): number {
	const id = nextId++;
	queue.push({
		id,
		callback,
		fireAt: performance.now() + Math.max(0, intervalMs),
		isInterval: true,
		intervalMs: Math.max(0, intervalMs),
	});
	return id;
}

export function clearInterval(id: number): void {
	clearTimeout(id);
}

waveforge.addEventListener('step', checkTimers);
