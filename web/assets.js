// SPDX-License-Identifier: GPL-3.0-or-later
export const REQUIRED = ['ALP2.mdl', 'ALP2.col', 'ALP2.gates', 'ALP2.start.pts', 'bike1.mdl'];
export const OPTIONAL = ['ALP2.sky.mdl', 'ALP2.dome.mdl'];
export const MAX_FILE = 256 * 1024 * 1024;
export const MAX_TOTAL = 320 * 1024 * 1024;
export function selectAssets(files) {
  const allowed = new Map([...REQUIRED, ...OPTIONAL].map(name => [name.toLowerCase(), name]));
  const selected = new Map();
  let total = 0;
  for (const file of files) {
    const name = allowed.get(file.name.toLowerCase());
    if (!name) throw new Error(`Archivo no admitido: ${file.name}. Selecciona únicamente el paquete ALP2 convertido; las ISO no se cargan aquí.`);
    if (selected.has(name)) throw new Error(`Archivo duplicado: ${name}`);
    if (!file.size || file.size > MAX_FILE) throw new Error(`${name}: tamaño vacío o superior a 256 MiB.`);
    total += file.size;
    if (total > MAX_TOTAL) throw new Error('El paquete supera el presupuesto de 320 MiB de esta iteración.');
    selected.set(name, file);
  }
  const missing = REQUIRED.filter(name => !selected.has(name));
  if (missing.length) throw new Error(`Faltan archivos: ${missing.join(', ')}.`);
  return { selected, total };
}
