# QTerm2

Terminal para Linux inspirado en iTerm2, escrito en C++17 con Qt 6 Widgets.
Incluye un gestor de sesiones SSH con autenticación por llave pública, panel
lateral plegable, pestañas, paneles divididos y un emulador xterm-256color
propio (sin depender de qtermwidget ni libvterm).

La arquitectura por capas, el esquema de datos, el modelo de seguridad y la
hoja de ruta están documentados en [`ARCHITECTURE.md`](ARCHITECTURE.md).

```
┌────────────────────────┬─────────────────────────────────────────────┐
│ SESIONES            +  │  Producción — QTerm2                        │
│ [buscar servidor…]     │  ┌─ pestañas ────────────────────────────┐  │
│ Servidores · 2         │  │ ● Producción │ ● NAS │ ● Local       │  │
│   ● Producción         │  └───────────────────────────────────────┘  │
│   ● NAS de casa        │  usuario@servidor:~$ _                      │
│ Locales · 1            │                                             │
│   ● Terminal local     │                                             │
│ [Terminal local][SSH]  │                                             │
└────────────────────────┴─────────────────────────────────────────────┘
```

## Características

- **Emulador de terminal**: xterm-256color, color de 24 bits, UTF-8, pantalla
  alternativa, regiones de scroll, DECSCUSR, portapapeles, bracketed paste,
  reporte de foco, ratón SGR (1000/1002/1003/1006), títulos OSC 0/1/2 y
  directorio actual OSC 7.
- **Historial** configurable por líneas (10 000 por defecto) y por memoria
  (64 MB por defecto), búsqueda incremental y selección con ratón; doble clic
  selecciona palabras.
- **SSH mediante OpenSSH del sistema**: llave pública, ssh-agent o contraseña
  interactiva. Soporta puerto, ProxyJump, `-X`, `-A`, keepalive, timeout,
  política de host key, comando inicial, túneles y argumentos ssh adicionales.
- **Integración con OpenSSH**: importa hosts concretos desde `~/.ssh/config`,
  exporta el catálogo a un bloque gestionado, revisa o depura
  `~/.ssh/known_hosts` y muestra las identidades cargadas en el agente SSH.
- **Reconexión opcional**: por sesión, reintenta hasta 5 veces con espera
  creciente (1, 2, 4, 8 y 16 s) cuando el proceso SSH termina inesperadamente.
- **Perfiles de terminal por sesión**: fuente, tamaño, scrollback, forma de
  cursor y parpadeo propios, con la preferencia global como respaldo.
- **Snippets**: comandos reutilizables con `${host}`, `${user}`, `${port}`,
  `${name}` y `${group}`. Se insertan en la terminal activa para revisarlos
  antes de ejecutar; nunca se lanzan solos.
- **Búsqueda global**: localiza texto en el scrollback de todas las pestañas y
  paneles, y salta a la coincidencia en su terminal.
- **Plugins v2**: Sistema de plugins con API v2, permisos, soporte para paneles/temas/backends. Plugin de Docker/Kubernetes incluido.
- **Panel de transferencias SFTP**: UI con cola, progreso, pausa/reanudación/cancelación.
- **Editor remoto seguro**: Descarga temporal, edición local, subida segura.
- **Gestor de túneles UI**: Interfaz para túneles -L/-R/-D con estado y control.
- **SFTP**: Servicio SFTP integrado con panel de transferencias, cola, progreso y resume.
- **Historial de comandos**: Historial por sesión persistente y searchable.
- **Túneles gestionados**: Túneles -L/-R/-D con estado, habilitado/deshabilitado y reconexión automática.
- **Shell Integration (OSC 133)**: Soporte para shell integration compatible con iTerm2.
- **Triggers**: Reglas regex para reaccionar a la salida del terminal.
- **Broadcast Input**: Escribir simultáneamente en múltiples paneles.
- **Status Bar configurable**: Barra de estado modular con componentes (hostname, git, reloj, CPU, memoria).
- **Sincronización cifrada**: Exportar/importar sesiones con cifrado AES.
- **Empaquetado**: Soporte para AppImage y Flatpak.
- **Command Palette**: `Ctrl+Shift+P` abre un buscador de acciones por título,
  categoría o atajo. Ejecuta tanto comandos nativos como los que expongan los
  plugins a través del mismo registro.
- **Gestor de sesiones**: alta/edición/duplicado/borrado, grupos, etiquetas de
  color, filtro y menú contextual. Se guarda en SQLite.
- **Favoritos y recientes**: grupos virtuales en el panel lateral. Los
  favoritos se alternan desde el menú contextual y los recientes muestran las
  últimas 8 sesiones usadas.
- **Túneles**: editor de port forwards `-L` (local), `-R` (remoto) y `-D`
  (SOCKS5 dinámico) con host/puerto de escucha, destino y activación por fila.
- **Llaves**: generación de llaves ed25519 con `ssh-keygen` e instalación de la
  llave pública con `ssh-copy-id` dentro de una terminal.
- **Interfaz**: panel lateral ocultable, pestañas reordenables, paneles
  divididos horizontal/vertical, temas oscuro y claro, zoom de fuente.
- **Cursor configurable**: forma de bloque, I-Beam (barra de celda completa) o
  subrayado, con parpadeo activable desde Preferencias.
- **Enlaces**: Ctrl+clic sobre una URL la abre en el navegador.
- **Seguridad**: las contraseñas nunca se guardan; OSC 52 (escritura remota del
  portapapeles) se ignora deliberadamente.

## Requisitos

- Linux con Qt 6.2 o superior (probado con Qt 6.12 en openSUSE Tumbleweed).
- CMake ≥ 3.16, un compilador C++17 y `g++`.
- OpenSSH (`ssh`, `ssh-keygen`, `ssh-copy-id`) en el `PATH`.
- Fuente monoespaciada (se elige automáticamente entre JetBrains Mono, Hack,
  Fira Code, Cascadia, Source Code Pro, DejaVu Sans Mono, etc.).
- Opcional: zsh para la experiencia interactiva recomendada.
- Opcional: `libsecret-1` para guardar secretos en el Secret Service del
  escritorio. Sin ella, QTerm2 compila y funciona, pero no persiste secretos.

En openSUSE:

```sh
sudo zypper install cmake gcc-c++ qt6-base-devel openssh zsh
```

## Compilar y ejecutar

```sh
cmake -S ~/Desktop/QTerm2 -B ~/Desktop/QTerm2/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/Desktop/QTerm2/build -j"$(nproc)"
~/Desktop/QTerm2/build/QTerm2
```

Instalación en el sistema (binario, lanzador e icono):

```sh
sudo cmake --install ~/Desktop/QTerm2/build --prefix /usr/local
```

También puede usarse el lanzador `packaging/QTerm2.desktop`.

## Primeros pasos

1. La primera vez se crea una sesión local y se abre una terminal.
2. Pulsa **Nueva SSH** en el pie del panel lateral (o `Ctrl+Shift+N`).
3. Escribe nombre, servidor, usuario y puerto. En **Autenticación**:
   - **Llave pública**: elige una llave existente o pulsa **Generar…** para
     crear una llave ed25519. Después guarda la sesión.
   - **Agente SSH**: usa `SSH_AUTH_SOCK`; añade la llave con `ssh-add`.
   - **Contraseña**: OpenSSH la pedirá dentro de la terminal. No se almacena.
4. Para autorizar una llave en el servidor, clic derecho sobre la sesión →
   **Instalar llave pública en el servidor…**. Se abre `ssh-copy-id` en una
   terminal y se escribe la contraseña si hace falta.
5. En el menú contextual puedes **Añadir a favoritos**; la sesión aparecerá en
   el grupo *Favoritos*. Las últimas 8 usadas se muestran en *Recientes*.
6. En **Avanzado** ajusta la política de host key, el timeout y la reconexión
   automática. En **Túneles** añade forwards locales, remotos o SOCKS5 que se
   aplicarán al conectar. En **Terminal** activa un perfil propio de fuente,
   tamaño, scrollback y cursor para esa sesión.
7. El menú **Sesiones** permite importar hosts desde `~/.ssh/config`, exportar
   el catálogo a un bloque gestionado, abrir el visor de `known_hosts`,
   consultar las identidades del agente SSH y gestionar **Snippets**.
8. Doble clic en una sesión para conectar. El panel se oculta con `Ctrl+B` o
   con el botón `‹`.

Las sesiones, los ajustes de la interfaz y la metadata se guardan en una base
de datos SQLite:

```
~/.local/share/QTerm2/qterm2.db
```

En el primer arranque se importan automáticamente las sesiones de
`~/.config/QTerm2/sessions.json` y la configuración de
`~/.config/QTerm2/QTerm2.conf`; los archivos originales se renombran a
`*.migrated-<fecha>` y no se vuelven a leer.

## Atajos

| Atajo | Acción |
| --- | --- |
| `Ctrl+T` | Nueva terminal local |
| `Ctrl+Shift+N` | Nueva conexión SSH |
| `Ctrl+W` | Cerrar pestaña |
| `Ctrl+Shift+W` | Cerrar panel dividido |
| `Ctrl+Shift+D` | Dividir a la derecha |
| `Ctrl+Shift+E` | Dividir abajo |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Pestaña siguiente / anterior |
| `Ctrl+1…9` | Ir a la pestaña |
| `Ctrl+B` | Mostrar/ocultar panel lateral |
| `Ctrl+Shift+P` | Command Palette |
| `Ctrl+Shift+S` | Snippets |
| `Ctrl+Shift+F` | Buscar |
| `Ctrl+Shift+G` | Buscar en todas las pestañas |
| `F3` / `Shift+F3` | Siguiente / anterior coincidencia |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | Copiar / pegar |
| `Ctrl+Shift+A` | Seleccionar todo |
| `Ctrl+Shift+K` | Limpiar historial |
| `Ctrl++` / `Ctrl+-` / `Ctrl+0` | Zoom / tamaño normal |
| `Shift+PageUp` / `Shift+PageDown` | Desplazar historial |
| `Ctrl+clic` | Abrir URL |

La forma del cursor y el parpadeo se ajustan en **Editar → Preferencias**.
También se guardan en la base de datos como `terminal/cursorStyle` y
`terminal/cursorBlink`.

## Seguridad

- Las contraseñas SSH se escriben directamente en el PTY y no se guardan en
  disco, en la base de datos ni en ningún archivo de sesiones.
- QTerm2 no implementa SSH: delega en el cliente OpenSSH, por lo que se
  respetan `~/.ssh/config`, `known_hosts`, ProxyJump y las políticas del
  servidor.
- Las llaves generadas quedan con permisos `0600` y su `.pub` con `0644`.
- La exportación a `~/.ssh/config` solo modifica el bloque delimitado por
  QTerm2, conserva el resto del archivo, crea una copia de seguridad y escribe
  con permisos `0600`.
- El visor de `known_hosts` también hace copia de seguridad y reescribe con
  permisos `0600`; las entradas con hash solo se pueden eliminar por línea.
- OSC 52 se ignora para impedir que un programa remoto sobrescriba el
  portapapeles local sin intervención del usuario.
- Al cerrar una pestaña con procesos activos se pide confirmación (se puede
  desactivar en Preferencias).

## Estructura del proyecto

```
CMakeLists.txt
ARCHITECTURE.md       Arquitectura, interfaces, seguridad y hoja de ruta
src/
  main.cpp            Arranque y creación del ApplicationContext
  app/                Composition root
  core/               Rutas, ProcessSpec, registro de comandos
  domain/             Modelo Session, PortForward y snippets
  storage/            SQLite, repositorios, ajustes y migración legada
  security/           CredentialStore y backend libsecret opcional
  ssh/                Comandos SSH, llaves, ssh_config, known_hosts y agente
  sessions/           Lanzador y fachadas SessionStore/SnippetStore
  terminal/           Emulador VT, transporte PTY y vista TerminalWidget
  plugins/            API y cargador de plugins
  ui/                 MainWindow, sidebar, paneles, diálogos y tema
tests/
  smoketest.cpp       Sesiones, SQLite, PTY, parser, zsh y llaves
  uitest.cpp          Ventana principal headless, panel lateral y tema
  archtest.cpp        Fronteras de arquitectura, persistencia y seguridad
packaging/
  QTerm2.desktop
  QTerm2.svg
```

## Pruebas

```sh
ctest --test-dir ~/Desktop/QTerm2/build --output-on-failure
```

Las pruebas se ejecutan con `QT_QPA_PLATFORM=offscreen`. La prueba de humo
arranca `/bin/sh` y, si está disponible, una zsh interactiva en un PTY real.
La prueba de arquitectura cubre la base de datos, la migración legada, los
repositorios, el almacén de credenciales, la construcción de comandos SSH, el
registro de comandos y el emulador sin interfaz. Ninguna prueba toca la base de
datos real del usuario.

## Limitaciones conocidas

- No se emulan sixel, Kitty graphics, iTerm2 inline images ni OSC 52.
- Los caracteres fuera del BMP (emoji astrales) se muestran como `�`; los
  caracteres anchos del BMP (CJK) sí ocupan dos celdas.
- No hay restauración automática de sesiones al reiniciar.
- No se incluyen cliente SSH propio, serial, telnet ni RDP.
- Solo probado en Linux/X11; Qt 6 permite portarlo, pero no es el objetivo.
