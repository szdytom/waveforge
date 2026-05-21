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
			js: '',
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
		// Only keep outputs keys — C++ only iterates file paths from outputs
		const metaPath = join(OUT_DIR, '.metafile.json');
		const simpleMeta = { outputs: {} };
		for (const key of Object.keys(result.metafile.outputs)) {
			simpleMeta.outputs[key] = {};
		}
		writeFileSync(metaPath, JSON.stringify(simpleMeta, null, 2));
		console.log(`Wrote metafile → ${metaPath}`);

		// Keep full metafile for debugging
		const fullMetaPath = join(OUT_DIR, '.metafile.full.json');
		writeFileSync(fullMetaPath, JSON.stringify(result.metafile, null, 2));
		console.log(`Wrote full metafile → ${fullMetaPath}`);

		console.log(`Built ${entries.length} scene(s) → ${OUT_DIR}`);
	}
}

build().catch((err) => {
	console.error('Build failed:', err);
	process.exit(1);
});
