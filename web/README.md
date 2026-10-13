# Downhill Domination Web — primera iteración

Port experimental del prototipo C++ de DownHill-Port-PC. No es el juego completo.
La simulación, colisiones y renderizado se ejecutan localmente en WebAssembly y
WebGL 2; el servidor entrega archivos estáticos. No se cargan ISO en esta versión.

## Tu versión local con ALP2 preparado

Ya se extrajeron de la ISO del usuario ALP2 y la bicicleta. El servidor local
detecta `out/local-game/manifest.json`, carga automáticamente esos recursos y
oculta el botón de archivos. Espera la carga y pulsa **Jugar**. Solo se sirven los
siete archivos convertidos permitidos: la ISO y los contenedores no se exponen.

Para repetir la preparación en Windows:

```powershell
.venv/Scripts/python.exe -m pip install pycdlib==1.22.0 numpy pillow
.venv/Scripts/python.exe tools/prepare_local_game.py "Downhill Domination.iso"
python tools/serve_web.py
```

Linux: usa `.venv/bin/python` para las dos primeras instrucciones. El conversor
lee únicamente ALP2/CANFIELD/MAN100/WHBRIAN mediante pycdlib y reutiliza los
conversores originales. No modifica ni transmite la ISO. Cada recurso preparado
se comprueba en el navegador contra su tamaño y SHA-256 local.

`serve_web.py --public` desactiva esta carga privada para probar la interfaz
pública. La build estática y `dist/` llevan `local-assets.json` desactivado y no
contienen esos recursos. No copies `out/local-game/` a un despliegue.

Consulta la [auditoría](../docs/web-audit.md) y el
[registro de verificación](../docs/web-verification.md) para distinguir código
implementado, pruebas sintéticas y comprobaciones pendientes con ALP2 real.

## SDK y compilación reproducible

Requisitos: Python 3.11+, Git, CMake 3.20+, Ninja, Node para los tests.
Versión comprobada: Emscripten **6.0.12**, SDL3 **3.2.26**
(`badbf8da4ee72b3ef599c721ffc9899e8d7c8d90`). SDL se compila desde fuente, sin
el port SDL2 ni una dependencia OpenGL de escritorio en el objetivo web.

Desde la raíz:

```sh
git clone https://github.com/emscripten-core/emsdk.git .tools/emsdk
```

Windows PowerShell:

```powershell
.tools/emsdk/emsdk.bat install 6.0.12
.tools/emsdk/emsdk.bat activate 6.0.12
python -m venv .venv
.venv/Scripts/python.exe -m pip install ninja
python tools/build_web.py --test
python tools/serve_web.py
```

Linux:

```sh
.tools/emsdk/emsdk install 6.0.12
.tools/emsdk/emsdk activate 6.0.12
python3 -m venv .venv
.venv/bin/python -m pip install ninja
python3 tools/build_web.py --test
python3 tools/serve_web.py
```

El script encuentra el SDK local sin modificar tu PATH global. También acepta
`--sdk RUTA`, `--build RUTA`, `--sdl-source RUTA` y `--debug`. CMake descarga SDL
en la primera configuración si no se proporciona su fuente; necesita red.
Para trabajar offline, clona antes `release-3.2.26` en `.tools/SDL`.

La salida es `build-web/public`: HTML, CSS, JS, WASM y licencias. No se precargan
recursos propietarios ni se copian `out/`, `unpacked/` o la ISO.
Abre **http://127.0.0.1:8080**, no index.html mediante file://.
[Instalación oficial Emscripten](https://emscripten.org/docs/getting_started/downloads.html).

Equivalente manual con el entorno emsdk activado:

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel 4
ctest --test-dir build-web --output-on-failure
node tests/web_assets_test.mjs
```

## Conversión offline de tu copia

Instala 7-Zip, NumPy y Pillow. Puedes usar el flujo original:

```sh
python tools/dh.py setup "TU_COPIA.iso" --levels ALP2
```

Ese comando también intenta compilar el visor nativo; requiere sus dependencias.
Si ya tienes los contenedores descomprimidos en `unpacked/`, evita esa compilación:

```sh
python -c "from pathlib import Path; Path('out/maps').mkdir(parents=True, exist_ok=True); Path('out/play').mkdir(parents=True, exist_ok=True)"
python tools/extract_model.py unpacked/LVL/ALP2 out/maps/ALP2.mdl
python tools/collision.py unpacked/LVL/ALP2.NGP out/maps/ALP2.col --instances
python tools/markers.py unpacked/LVL/ALP2.NGP x out/maps/ALP2.start.pts out/maps/ALP2.gates
python tools/extract_model.py unpacked/BIKE/CANFIELD out/play/frame.mdl
python tools/extract_model.py unpacked/BIKE/MAN100 out/play/bars.mdl
python tools/extract_model.py unpacked/BIKE/WHBRIAN out/play/wheel.mdl
python tools/assemble_bike.py out/play/bike1.mdl out/play/frame.mdl out/play/bars.mdl out/play/wheel.mdl
```

`extract_model.py` puede generar `ALP2.sky.mdl` y `ALP2.dome.mdl` junto al terreno.
La bici ensamblada ya está en Y arriba: no uses `--keep-axes`.
No se requiere `chain0.pts` para el modo de conducción manual.
La conversión de ALP2 PAL desde la ISO aportada fue comprobada en este equipo.

Selecciona en un mismo diálogo estos archivos (cópialos a una carpeta privada
común si están en carpetas distintas):

| Obligatorios | Opcionales |
|---|---|
| ALP2.mdl, ALP2.col, ALP2.gates, ALP2.start.pts, bike1.mdl | ALP2.sky.mdl, ALP2.dome.mdl |

El botón Jugar se habilita tras validar y subir la geometría a la GPU. Hay
rechazo de versiones, truncamiento, índices inválidos, NaN y tamaños excesivos.
Los hashes SHA-256 del registro son identificación local; no prueban que todos
los archivos procedan del mismo disco. El formato original carece de ese vínculo.

## Controles y funcionamiento

W acelerar, S frenar, A/D girar, Q/E inclinar, Espacio saltar,
Enter reaparecer en el último punto seguro, T reiniciar, Mayús pedaleo extra.
Haz clic en el canvas para enfocar. Al perder el foco se pausa; Continuar reanuda.
Se conserva `Race/Run/Bike`, la cámara de seguimiento y el paso fijo **1/60 s**
del visor. Las hipótesis de `BikeParams` siguen siendo hipótesis.

Los ajustes Bajo/Medio cambian la resolución interna (540p/720p). No alteran
la física ni prometen 60 FPS. El renderer usa buffers GPU estáticos, tandas
consecutivas por textura y visibilidad DHM3. Sin SIMD, hilos, Asyncify ni servidor
de simulación. La GPU se reconstruye desde las copias CPU al recuperar contexto.

Las métricas muestran FPS, intervalo entre cuadros, tiempo CPU (incluye envío de
comandos, no tiempo GPU), capacidad del heap WASM y duración de carga. El heap
no incluye memoria JS/GPU ni representa bytes vivos. `window.downhillMetrics`
también expone llamadas de dibujo, grupos visibles y recortes de tiempo al
superar 100 ms; el juego se ralentiza en sobrecarga en vez de acumular trabajo.

## Límites de esta iteración

- En Chrome se comprobó carga automática, render y conducción inicial de ALP2
  real. Faltan recorrido completo, comparación visual con el visor y Edge.
- Carga por etapas con progreso de bytes y etapas terminadas; los parsers C++,
  construcción de rejilla y subida de cada modelo aún son síncronos. Con archivos
  grandes pueden producir pausas. No se afirma una carga completamente no bloqueante.
- Límite 256 MiB por archivo, 320 MiB por paquete, 1 GiB de heap WASM. Puede
  agotarse la memoria del navegador antes; reducir copias es trabajo pendiente.
- No se porta todavía el sesgo heurístico entre láminas coplanares del visor;
  transparencia y superposición requieren comparación con capturas propias.
- Cielo opcional; sin él aparece el color de limpieza, no una recreación.
- .msh/.pts sueltos, HUD texturizado, niebla, piloto, audio y gamepad pendientes.
  No hay multijugador ni pruebas de determinismo entre arquitecturas.
- Las builds nativas originales siguen usando SDL3/OpenGL. Linux no se ejecutó
  en este equipo Windows; no se declara comprobado.

## Distribución estática y GPL

Antes de distribuir, crea una carpeta que contenga únicamente archivos permitidos
y el código fuente correspondiente, incluyendo SDL:

```sh
python tools/build_web.py
python tools/package_web.py
```

`dist/` contiene los archivos públicos, `source.zip` y `SOURCE.txt` con hashes de
las fuentes actuales. El empaquetador no toma assets; usa una lista de archivos
públicos explícita y rechaza extensiones de recursos del juego en el archivo de
fuentes. No distribuyas `build-web/` completo, el SDK, la ISO ni tu carpeta privada.
La salida del paquete no constituye una publicación automática.

En Vercel despliega **solo dist/** como sitio estático (Framework: Other, sin
Build Command), por Vercel Drop o CLI desde esa carpeta: `vercel deploy`, revisa
la preview y después `vercel deploy --prod`. No son necesarios COOP/COEP porque
esta build no usa SharedArrayBuffer/pthreads. Comprueba MIME `application/wasm`
y evita cachear JS de una build con WASM de otra.
[Despliegues oficiales de Vercel](https://vercel.com/docs/deployments).

Conserva LICENSE, avisos de autoría y los cambios identificados como port web.
Al distribuir JS/WASM derivados GPL, facilita el código fuente correspondiente
de **esa misma versión**, herramientas/instrucciones para compilar y dependencias,
bajo GPL-3.0-or-later; no basta enlazar al upstream sin tus cambios.
La GPL del motor no concede derechos sobre los datos del juego.
SDL se distribuye bajo zlib; el runtime Emscripten bajo MIT/NCSA (avisos incluidos).
Véanse las secciones 5 y 6 de [LICENSE](../LICENSE). El empaquetador incorpora SDL
y las fuentes del port; instala el SDK de la versión indicada al reconstruir.

## Próximo criterio de avance

Antes de planificar el juego completo: cargar un paquete privado ALP2 real,
comparar encuadre/materiales/cielo con dhview, recorrer sus puertas con teclado,
medir carga/memoria y ensayar pérdida de contexto en Chrome/Edge.
Gamepad API queda después de esa validación. El piloto, selección, menús,
IA, audio, guardado y multijugador no se implementan ni se consideran desbloqueados
por las pruebas sintéticas de esta iteración.
