import * as esbuild from 'esbuild';
import { readdirSync, mkdirSync, writeFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, '..');
const SCENES_DIR = join(ROOT, 'scenes');
const OUT_DIR = join(ROOT, 'assets', 'bundled-js');

const isWatch = process.argv.includes('--watch');

async function build() {
	mkdirSync(OUT_DIR, { recursive: true });

	const entries = readdirSync(SCENES_DIR)
		.filter(f => (f.endsWith('.ts') || f.endsWith('.tsx')) && !f.endsWith('.d.ts'))
		.map(f => join(SCENES_DIR, f));

	if (entries.length === 0) {
		console.log('No scene entry points found in scenes/');
		return;
	}

	const ctx = await esbuild.context({
		entryPoints: entries,
		outdir: OUT_DIR,
		outbase: SCENES_DIR,
		target: 'es2020',
		format: 'esm',
		platform: 'neutral',
		bundle: true,
		splitting: true,
		minify: true,
		treeShaking: true,
		entryNames: '[name]',
		chunkNames: 'chunks/[name]-[hash]',
		metafile: true,
		mainFields: ['main'],
		loader: {
			'.txt': 'text',
			'.ts': 'tsx',
		},
		jsx: 'automatic',
		jsxImportSource: 'react',
		define: {
			'process.env.NODE_ENV': '"production"',
		},
		banner: {
			js: 'globalThis.process??={};globalThis.process.env??={};globalThis.process.env.NODE_ENV="production";globalThis.console??={};globalThis.console.warn=waveforge.log;',
		},
		sourcemap: false,
		logLevel: 'info',
	});

	if (isWatch) {
		await ctx.watch();
		console.log('Watching for changes...');
	} else {
		const result = await ctx.rebuild();
		await ctx.dispose();

		// Write metafile for C++ module loader
		const metaPath = join(OUT_DIR, '.metafile.json');
		writeFileSync(metaPath, JSON.stringify(result.metafile, null, 2));
		console.log(`Wrote metafile → ${metaPath}`);

		console.log(`Built ${entries.length} scene(s) → ${OUT_DIR}`);
	}
}

build().catch((err) => {
	console.error('Build failed:', err);
	process.exit(1);
});
