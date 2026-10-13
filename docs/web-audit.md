# Auditoría del port web — 2026-10-13

Base examinada: `aad7da47416dd0699832f3dcea85ceee5bcea624` de
[DownHill-Port-PC](https://github.com/Pedroj-64/DownHill-Port-PC).
Auditoría realizada antes de editar el motor y su compilación.

## Viabilidad y límites

Es viable portar el prototipo C++ existente; no equivale a portar el juego
completo. README y docs/en/architecture.md describen una reconstrucción parcial.
La arquitectura conserva texto histórico desactualizado (main.cpp ya tiene más
de 500 líneas). No se usan sus porcentajes como pruebas de esta iteración.

`src/main.cpp` y `src/gl_renderer.hpp` dependen de OpenGL de compatibilidad:
glBegin/glEnd, GL_QUADS, matrices implícitas, arrays de cliente, glTexEnv y niebla
fija. No son GLES3. Se necesitan shaders GLSL ES 300, matrices explícitas,
VAO/VBO/EBO, triángulos y samplers. La geometría .mdl incluye color, UV y textura;
la visibilidad DHM3 y las capas de cielo deben conservarse. El sesgo de profundidad
heurístico entre láminas del visor requiere comparación visual posterior.

CMake original exige SDL3 y OpenGL para todo, incluso tests sin gráficos. El
`#include <bits/basic_string.h>` de main.cpp es exclusivo de libstdc++ y sobra.
El bucle while del visor bloquea el navegador. Un punto de entrada web separado
evita alterar el visor nativo; comparte las cabeceras del motor y usa rAF mediante
emscripten_set_main_loop. No necesita Asyncify ni pthreads.

## Componentes reutilizables

- Ground: .col, transformación Z arriba → Y arriba, índice espacial, barridos.
- Contact: respuesta, despenetración y cuerpo rígido.
- Bike/Run: conducción, salto, última posición segura y puertas. Se conserva el
  integrador jugable aproximado; integrator_fidel no sustituye automáticamente
  la conducción. Validar vuelo libre no valida contactos ni el juego entero.
- Controls: empaquetado de entradas y flanco de salto. Race: cuenta atrás y
  contrarreloj provisional; conservar paso fijo de 1/60 s del visor.
- Renderer: interfaz independiente de GL, utilizable como contrato GLES3.
- Rider/NGA: existen parsers y skinning parcial; no se añaden al prototipo antes
  de verificar terreno y bici. Audio, rivales y juego completo no disponibles.

## Recursos y herramientas examinadas

`dh.py setup`: 7z extrae la ISO local; unpack_ie.py descomprime IE/deflate;
extract_model.py usa scene/vif/gif/gs y PTR/TEX/RTX para convertir escena,
instancias y texturas RGBA a DHM2/DHM3; exporta sky/dome por separado.
extract_mesh.py produce .msh sin materiales; extract_points.py es heurístico.
collision.py produce .col (40 bytes/triángulo); markers.py produce .gates y
rejilla .start.pts; pts_path.py produce la ruta. assemble_bike.py ensambla
cuadro, horquilla y ruedas, y entrega por defecto la bici en Y arriba.
dh.py selecciona CANFIELD/MAN100/WHBRIAN. Estos procesos siguen offline.

ALP2 requiere modelo, colisión, puertas, rejilla y bici convertidos. Los nombres
no prueban procedencia; solo permiten seleccionar el paquete. Validar tamaños,
índices, números finitos y límites evita interpretar archivos incompatibles;
un SHA-256 local sirve como identificación, no acredita autenticidad sin un
manifiesto de referencia. No hay recursos del usuario en este entorno.

## SDL3 / Emscripten

Al comenzar no había emcc ni compilador C++ en PATH. Se prepara un SDK local.
SDL3 no debe confundirse con el antiguo -sUSE_SDL=2. Se compila una revisión
fijada de SDL3 desde código mediante CMake para el mismo toolchain.
[SDL documenta Emscripten y GLES3/WebGL2](https://wiki.libsdl.org/SDL3/README-emscripten).
[Emscripten documenta las restricciones GL](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html).
La compatibilidad efectiva se determinará por compilación, no por asumirla.

## Pruebas iniciales

Examinados tests C++ de física/controles/carrera e integrador, tests Python de
formatos y CI guard. `python -m unittest discover -s tests`: 92 tests,
1 fallo, 20 errores, 3 omitidos. Faltan numpy y permisos para algunos temporales;
otros tests dependen de PCSX2/datos externos. Se registrará la verificación final
por separado, con dependencias y temporales controlados.

La visualización y conducción reales de ALP2 siguen pendientes hasta disponer
de los recursos privados y probar en Chrome/Edge. No se distribuyen ISO ni
assets; las salidas de desarrollo y SDK quedan ignoradas.
