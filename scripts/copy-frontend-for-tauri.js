// Copies just the static files Warf needs to run inside Tauri into src-tauri/dist/. This exists
// because "frontendDist" can't point at the repo root (it would recursively embed src-tauri/target,
// node_modules, and .git into the binary). Not a real build step - just a file copy, no bundler -
// same approach and reason as Kerf's own scripts/copy-frontend-for-tauri.js.
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const DIST = path.join(ROOT, 'src-tauri', 'dist');

// Tauri expects an index.html inside frontendDist; the actual tool lives in app.html at the repo
// root (index.html there is the marketing landing page, not shipped in the desktop app).
const RENAMES = { 'app.html': 'index.html' };
const FILES = ['app.html', 'manifest.json'];

fs.rmSync(DIST, { recursive: true, force: true });
fs.mkdirSync(DIST, { recursive: true });

for (const file of FILES) {
    const destName = RENAMES[file] || file;
    fs.copyFileSync(path.join(ROOT, file), path.join(DIST, destName));
}

console.log(`[copy-frontend-for-tauri] copied to ${DIST}`);
