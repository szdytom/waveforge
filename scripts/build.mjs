import { mkdirSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import * as esbuild from 'esbuild';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, '..');
const SCENES_DIR = join(ROOT, 'scenes');
const OUT_DIR = join(ROOT, 'assets', 'bundled-js');

const isWatch = process.argv.includes('--watch');

function printBuildSummary(metafile, outDir) {
	const outputs = metafile.outputs;
	const entries = [];
	const chunks = [];

	for (const [file, info] of Object.entries(outputs)) {
		if (!file.endsWith('.js')) {
			continue;
		}
		const size = info.bytes;
		const isEntry = !!info.entryPoint;
		const item = { file, size, isEntry };
		if (isEntry) {
			entries.push(item);
		} else {
			chunks.push(item);
		}
	}

	entries.sort((a, b) => a.file.localeCompare(b.file));
	chunks.sort((a, b) => b.size - a.size);

	const totalBytes = [...entries, ...chunks].reduce((sum, i) => sum + i.size, 0);
	const totalKB = (totalBytes / 1024).toFixed(1);

	console.log('\n📦 Build Summary');
	console.log('='.repeat(50));

	if (entries.length) {
		console.log('\n🎬 Entry files:');
		entries.forEach(({ file, size }) => {
			console.log(`   ${file}\t→\t${(size / 1024).toFixed(2)} KB (${size} bytes)`);
		});
	}

	if (chunks.length) {
		console.log('\n📦 Shared chunks:');
		chunks.forEach(({ file, size }) => {
			console.log(`   ${file}\t→\t${(size / 1024).toFixed(2)} KB (${size} bytes)`);
		});
	}

	console.log(`\n${'='.repeat(50)}`);
	console.log(`✅ Total: ${totalKB} KB (${totalBytes} bytes)`);
	console.log(`📁 Output directory: ${outDir}\n`);
}

async function build() {
	mkdirSync(OUT_DIR, { recursive: true });

	const entries = readdirSync(SCENES_DIR)
		.filter((f) => (f.endsWith('.ts') || f.endsWith('.tsx')) && !f.endsWith('.d.ts'))
		.map((f) => join(SCENES_DIR, f));

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

		printBuildSummary(result.metafile, OUT_DIR);
	}
}

build().catch((err) => {
	console.error('Build failed:', err);
	process.exit(1);
});
