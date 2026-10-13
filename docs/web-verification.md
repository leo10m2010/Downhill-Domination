# Verificación de esta iteración

Fecha: 2026-10-13. Base upstream: `aad7da47416dd0699832f3dcea85ceee5bcea624`.
Cambios de este checkout sin publicar; las fuentes versionables no contienen
datos originales del juego. Los recursos privados aportados después se ignoran.

## Actualización: ISO aportada y ALP2 real

El usuario proporcionó su ISO local después de la primera verificación. Se leyó
la estructura ISO9660 y se extrajeron solo 16 contenedores de ALP2 y tres piezas
de bicicleta. La ISO no se modificó ni se envió a ningún servidor.
Los recursos derivados permanecen en `out/local-game/`, ignorado por Git.

Conversión comprobada: 426.907 triángulos del escenario, 3.819 chunks,
139 texturas, 136.176 triángulos de colisión, 28 puertas de recorrido y una
bici de 2.914 triángulos. Terreno, cielo, panorama, colisión, puertas, rejilla y
bici suman 74,8 MiB. Se corrigió la detección de la carpeta LVL del conversor en
Windows para conservar instancias y visibilidad DHM3.

Chrome cargó automáticamente los siete archivos reales, verificó sus hashes,
mostró terreno texturizado, objetos y bici, y permitió acelerar en el inicio de
ALP2 sobre la física C++ existente. glGetError=0; consola sin errores/advertencias
en las pruebas reales, incluida pérdida/restauración WebGL con estas mallas.
La medición corta registró ~156 MiB de capacidad de
heap WASM y 2.042 llamadas de dibujo iniciales; no constituye un benchmark del
recorrido completo ni una garantía de FPS. Captura local inspeccionada.

Se añadió `tests/web_local_browser_check.js` para repetir carga y conducción
con el paquete privado. El servidor loopback oculta el botón y el menú de carga
manual. La build pública conserva carga manual y una configuración local
desactivada, y el empaquetador excluye ISO/recursos. El test de rutas del servidor
comprobó rechazo de la ISO y recorridos `..` fuera de los archivos permitidos.

Las menciones a falta de recursos al final de este informe describen la primera
iteración; ese bloqueo ya se resolvió. Siguen pendientes la fidelidad visual
comparada con el visor, el recorrido completo y Edge. No se afirma juego completo.

## Implementado

| Componente | Alcance |
|---|---|
| Objetivo `dhweb` | C++20 + SDL3 estático + Emscripten; JS/WASM reales |
| GLES3/WebGL2 | GLSL ES 300, VAO/VBO/EBO, matrices, materiales/color/UV, capas sky/dome, selectores DHM3 |
| Física y carrera | Cabeceras originales Bike/Run/Race/Ground/Contact/Controls, ajustes del visor; paso fijo 1/60 s |
| Plataforma | rAF, canvas, resolución interna, pausa por foco, reinicio/reaparición, cámara de seguimiento |
| Recursos locales | File API → MEMFS → parsers C++; validación de límites, formatos, índices y números; hashes locales |
| Interfaz | Preparación, Jugar condicionado, progreso de bytes/etapas, controles, fullscreen, errores, métricas |
| Contexto perdido | Pausa, abandono de handles invalidados y reconstrucción GPU desde modelos CPU |
| Distribución | Scripts build/serve/package; lista explícita de públicos, fuente correspondiente y avisos de licencia |

El modelo jugable sigue siendo aproximado donde upstream lo documenta así.
No se sustituyó por RideBody, animación pregrabada, emulador ni física JavaScript.

## Comprobado mediante ejecución

- **Compilación WASM Release** con Emscripten 6.0.12 y SDL3 3.2.26. Artefactos
  `build-web/public/dhweb.js` y `dhweb.wasm`. SDL configurado GLES ON, desktop GL
  OFF, pthreads OFF. CMake 4.3.1 advierte sobre librerías compartidas de Emscripten;
  esta build usa librerías estáticas y terminó.
- **13/13 tests C++ WASM en Node**, incluyendo ground/contact/bike/race/controls,
  HUD, integrador, rider, NGA y nuevo parser web.
- **Visor Windows compilado** con MSVC 19.51, Windows SDK 10.0.26100 y SDL3
  nativo 3.2.26; resultado `build-native/Release/dhview.exe`.
- **14/14 registros CTest nativos** terminan correctamente. `air_replay_check`
  informa SKIP internamente porque no hay DH_AIR_REF; ese resultado no verifica
  el oráculo con datos del juego.
- **106 tests Python**, OK con **5 omitidos** por condiciones de sus tests;
  NumPy 2.4.6/Pillow 12.3.0. Inicialmente hubo dependencias y temporales bloqueados.
  Usar C:/tmp para TEMP/TMP resolvió los fallos del guard de telemetría, pues el
  TEMP habitual quedaba bajo un repositorio Git padre. No se cambió ese guard.
- **Tests JavaScript** de selección, duplicados, falta de archivos y presupuesto:
  `node tests/web_assets_test.mjs`, OK. `git diff --check`, OK.
- **Chrome real mediante Playwright CLI**: inicio SDL/WebGL2; Jugar deshabilitado
  sin datos; rechazo de paquete incompleto; canvas 1280×720 → 960×540; carga de
  modelos/colisión/puertas sintéticos; render con llamadas GPU y glGetError=0;
  aceleración con teclado sobre física C++; pérdida/restauración WebGL mediante
  WEBGL_lose_context; reconstrucción, métricas nuevas y pausa al salir del canvas.
  Última ejecución: **0 errores, 0 advertencias de consola**.

Se detectó y corrigió en la prueba de contexto perdido la eliminación indebida
de objetos GL del contexto antiguo. También se corrigieron dos detalles de tests
originales: ruta `/tmp` exclusiva de Unix en controls_test y reserva masiva para
un archivo HUD inválido. No cambian la física ni el comportamiento del visor.

## Reproducir

```sh
python tools/build_web.py --test
node tests/web_assets_test.mjs
python tools/serve_web.py --public
```

Con Playwright CLI y Chrome instalado, en otra terminal:

```sh
npx --yes --package @playwright/cli playwright-cli -s=downhill open http://127.0.0.1:8080 --browser chrome
npx --yes --package @playwright/cli playwright-cli -s=downhill run-code --filename tests/web_browser_check.js
npx --yes --package @playwright/cli playwright-cli -s=downhill console
npx --yes --package @playwright/cli playwright-cli -s=downhill close
```

Para el paquete privado real, inicia `python tools/serve_web.py` sin `--public`
y usa `--filename tests/web_local_browser_check.js` en lugar del snippet sintético.

El snippet genera datos sintéticos en memoria **solo en tests**. No sustituye el
escenario ALP2 ni se ejecuta desde la interfaz distribuida. Su resultado JSON
separa métricas de esa geometría de cualquier medición futura con el juego.
En este entorno Windows, herramientas que generan subprocesos y el servidor
necesitaron permisos fuera del sandbox; los EPERM/404 iniciales no eran errores
del motor. Ejecuta build y servidor bajo el mismo usuario/contexto de permisos.

## Pendiente y bloqueado

**Bloqueado por falta de recursos privados:** conversión del disco, integridad
entre archivos del mismo nivel, render fiel de ALP2, sus texturas/objetos, bici
ensamblada auténtica y recorrido completo con sus puertas. No se afirma ninguno
de esos resultados. Sin el paquete real tampoco es posible medir rendimiento,
carga o memoria representativos.

Este párrafo es el estado histórico antes de recibir la ISO; consulta la
actualización al principio para la validación real posterior.

**Pendiente:** Edge, ejecución nativa Linux, fullscreen con usuario, comparación
de transparencias/láminas, pruebas de paquetes grandes, culling/tiempo GPU,
fraccionamiento adicional de los parsers y subidas GPU, gamepad y selección de
archivos mediante procesador de ISO en WASM. El procesador futuro debe mantener
lecturas locales, cuotas/descompresión limitada y jamás subir la ISO.

**No iniciado:** piloto integrado, selección de personajes/bicis, lógica completa
de menús/carrera, rivales, audio, guardado y multijugador. La hoja de ruta detallada
se pospone hasta validar el prototipo ALP2 real, como requiere el proyecto.

Esta entrega prueba viabilidad del motor C++/SDL3 en WASM y una ruta de carga y
render ejecutada con datos sintéticos. No prueba que Downhill Domination completo
ni el escenario original ya sean jugables en web.
