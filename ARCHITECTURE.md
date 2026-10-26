# Arquitectura de QTerm2

> Documento vivo. Describe la arquitectura **realmente implementada**, las
> fronteras que deben respetarse al añadir funcionalidades y el plan de
> evolución hacia un gestor de terminales SSH profesional.
>
> Versión del documento: 1.9 · Esquema de base de datos: 4 · API de plugins: 2

---

## 1. Objetivo y alcance

QTerm2 es un emulador de terminal y gestor de sesiones orientado a
administradores de sistemas, DevOps, SRE y desarrolladores. El objetivo no es
solo "abrir una shell", sino centralizar el trabajo diario:

- terminal local y sesiones SSH en la misma ventana;
- catálogo de servidores con grupos, favoritos, búsqueda y recientes;
- pestañas y divisiones;
- gestión de claves, `~/.ssh/config`, `known_hosts` y SSH Agent;
- túneles locales, remotos y dinámicos;
- SFTP, Docker y Kubernetes como extensiones naturales;
- perfiles, temas, snippets, historial y Command Palette;
- un sistema de plugins estable y seguro.

Esta primera fase **no implementa todas esas funciones**. Implementa la
arquitectura que permite añadirlas sin reescribir el núcleo.

### Lo que ya funciona

| Área | Estado |
|---|---|
| Terminal local con PTY real (`forkpty`) | Implementado |
| Parser VT/xterm propio (ANSI, UTF-8, 256/truecolor, alt screen, mouse, bracketed paste, OSC 0/7) | Implementado |
| Sesiones SSH vía `ssh` del sistema dentro del PTY | Implementado |
| Catálogo de sesiones en SQLite | Implementado |
| Grupos, favoritos, recientes, búsqueda y orden en la barra lateral | Implementado |
| Pestañas y splits | Implementado |
| Editor de sesiones, generación de claves Ed25519, `ssh-copy-id` | Implementado |
| Editor de port forwards `-L`/`-R`/`-D`, timeout y política de host key | Implementado |
| Importación/exportación de `~/.ssh/config` en bloque gestionado | Implementado |
| Visor y depuración de `~/.ssh/known_hosts` | Implementado |
| Consulta de identidades del SSH Agent | Implementado (solo lectura) |
| Reconexión automática opcional con backoff | Implementado |
| Búsqueda en scrollback, copiar/pegar, selección por ratón, enlaces con Ctrl+clic | Implementado |
| Scrollback acotado por líneas y por memoria | Implementado |
| Temas claro/oscuro | Implementado |
| Migración desde `QTerm2.conf` + `sessions.json` | Implementado |
| API de plugins y cargador | Implementado (v1, superficie mínima) |
| Secreto en Secret Service | Interfaz implementada; backend libsecret opcional |
| Command Palette, snippets, SFTP, túneles gestionados con estado, Docker/K8s | Command Palette y snippets implementados; el resto, pendiente |

---

## 2. Principios de diseño

1. **Una sola dirección de dependencias.** `domain` y `core` no conocen Qt
   Widgets ni OpenSSH. `ui` conoce servicios, nunca al revés.
2. **El terminal no sabe qué hay al otro lado.** El emulador consume bytes; el
   transporte los produce. SSH, PTY local, serial o un contenedor son
   intercambiables.
3. **Composición explícita.** Ningún widget construye bases de datos,
   repositorios ni cargadores de plugins. Todo entra por `ApplicationContext`.
4. **Los secretos nunca se serializan.** Ni en SQLite, ni en JSON, ni en
   `QSettings`, ni en logs.
5. **Los comandos se construyen como argumentos, no como cadenas de shell.**
   `ProcessSpec` transporta `program` + `args` y se ejecuta sin `sh -c`.
6. **Las fronteras se prueban.** `archtest` verifica persistencia, migración,
   seguridad, construcción de SSH, registro de comandos y el emulador sin UI.
7. **Compatibilidad de plugins por versión.** La interfaz pública no se amplía
   en sitio: se versiona.

---

## 3. Vista general

```
┌──────────────────────────────────────────────────────────────────────┐
│                         QTerm2 (ejecutable)                          │
│                              main.cpp                                │
└───────────────────────────────┬──────────────────────────────────────┘
                                │ crea
                    ┌───────────▼────────────┐
                    │  ApplicationContext    │  composition root
                    │  (qterm2_core)         │
                    └───┬───────┬───────┬────┘
        ┌───────────────┘       │       └────────────────┐
        ▼                       ▼                        ▼
┌───────────────┐     ┌──────────────────┐      ┌─────────────────┐
│   Storage     │     │    Security      │      │    Plugins      │
│ Database      │     │ CredentialStore  │      │ PluginManager   │
│ SessionRepo   │     │ Secret Service   │      │ CommandRegistry │
│ SettingsStore │     └──────────────────┘      └─────────────────┘
│ LegacyMigr.   │
└───────┬───────┘
        │
┌───────▼────────────────────────────────────────────────────────────┐
│                         Domain / Servicios                         │
│  Session · PortForward · ProcessSpec · SessionLauncher             │
│  ssh::buildConnectSpec · ssh::KeyManager · SessionStore            │
└────────────────────────────────────────────────────────────────────┘
        │
┌───────▼──────────────────────┐      ┌──────────────────────────────┐
│       Termal engine          │      │             UI               │
│  TerminalEmulator (modelo)   │◄─────│ TerminalWidget (vista)       │
│  TerminalSession (interfaz)  │      │ MainWindow · Sidebar         │
│  PtyTerminalSession          │      │ TerminalPane · diálogos      │
└──────────────────────────────┘      └──────────────────────────────┘
```

### Capas

| Capa | Directorios | Puede depender de | Nunca debe depender de |
|---|---|---|---|
| Dominio | `src/domain` | QtCore | QtWidgets, SQLite, OpenSSH, UI |
| Núcleo | `src/core` | QtCore | SQLite, UI, OpenSSH |
| Persistencia | `src/storage` | dominio, `Qt6::Sql` | UI |
| Seguridad | `src/security` | QtCore, libsecret opcional | UI, SQLite |
| SSH | `src/ssh`, `src/sessions` | dominio, `QProcess`, `QProcessEnvironment` | UI, SQLite |
| Terminal | `src/terminal` | QtWidgets, `util` (POSIX) | OpenSSH, SQLite, UI específica |
| Plugins | `src/plugins` | dominio, `CommandRegistry` | UI, implementación de SSH |
| Aplicación | `src/app` | todos los servicios anteriores | — |
| UI | `src/ui` | aplicación, servicios, terminal | SQL directo, `QSettings` directo |

`qterm2_core` agrupa las capas de servicio y es enlazado tanto por la
aplicación como por los tests. `qterm2_app` añade `MainWindow` y compañía.

---

## 4. Mapa de módulos

### 4.1 `core/`

| Archivo | Responsabilidad |
|---|---|
| `apppaths.{h,cpp}` | Rutas XDG: configuración, datos, caché y base de datos. `forRoot()` permite tests herméticos. |
| `processspec.{h,cpp}` | Describe un proceso a lanzar (`program`, `args`, `workDir`, entorno, error). |
| `commandregistry.{h,cpp}` | Registro central de comandos con id, título, categoría, atajo y callback. |
| `pathutils.{h,cpp}` | `~` y rutas de usuario. |
| `status.h` | Resultado `{ok, message}` para operaciones sin excepción. |

### 4.2 `domain/`

`Session` es un **objeto valor** con la metadata persistible de una sesión:
tipo (`Local`/`Ssh`), host, puerto, usuario, autenticación, clave, ProxyJump,
directorio inicial, comando inicial, shell, X11, agent forwarding, keepalive,
timeout, política de host key, port forwards, favorito, timestamps, color y
notas.

Invariantes:

- no contiene contraseñas ni passphrases;
- no sabe lanzar procesos (eso es `SessionLauncher`);
- `toJson()`/`fromJson()` se conservan **solo** para migrar el formato legado.

`snippet.{h,cpp}` define `Snippet` (nombre, comando, descripción) y
`expandSnippet()`, que resuelve los placeholders con los datos de una `Session`.
La expansión es una función pura y no toca la base de datos ni el terminal.

### 4.3 `storage/`

| Archivo | Responsabilidad |
|---|---|
| `database.{h,cpp}` | Conexión SQLite nombrada, PRAGMAs y migración por `PRAGMA user_version`. |
| `sessionrepository.h` | Interfaz neutral de catálogo (`loadAll`, `upsert`, `remove`, `markUsed`, `count`). |
| `sqlitesessionrepository.{h,cpp}` | Implementación SQL, mapeo de filas y normalización de `NULL` a `''` en columnas `NOT NULL`. |
| `settingsstore.{h,cpp}` | Ajustes tipados como `QVariant` serializado en BLOB. |
| `legacymigration.{h,cpp}` | Importación única de `QTerm2.conf` y `sessions.json`, con renombrado a `.migrated-<timestamp>`. |
| `snippetrepository.h` / `sqlitesnippetrepository.{h,cpp}` | Catálogo neutral de snippets y su implementación SQLite. |

### 4.4 `security/`

- `credentialstore.h`: interfaz `CredentialStore` + `UnavailableCredentialStore`
  + `MemoryCredentialStore` (solo tests).
- `libsecretcredentialstore.{h,cpp}`: backend Secret Service, compilado solo si
  `pkg-config libsecret-1` está disponible (`QTERM2_HAVE_LIBSECRET`).

### 4.5 `ssh/` y `sessions/`

- `sshcommandbuilder.{h,cpp}`: construye el `ProcessSpec` de `ssh` y el de
  `ssh-copy-id`; encapsula todas las opciones.
- `sshconfig.{h,cpp}`: parser de `~/.ssh/config` (incluidos `Include`), conversión
  a sesiones y exportación a un bloque gestionado sin tocar el resto del archivo.
- `knownhosts.{h,cpp}`: lectura de `known_hosts`, detección de entradas con hash
  y eliminación por línea con copia de seguridad.
- `sshagent.{h,cpp}`: consulta `SSH_AUTH_SOCK` y `ssh-add -l`, y parsea las
  identidades. Es estrictamente de solo lectura: no carga ni retira llaves.
- `keymanager.{h,cpp}`: listar claves, obtener la pública, generar Ed25519,
  verificar permisos. **Nunca** lee material privado.
- `sessionlauncher.{h,cpp}`: decide entre shell local y conexión SSH.
- `sessionstore.{h,cpp}`: fachada en memoria sobre `SessionRepository` para la
  UI (señales, búsqueda, grupos, orden).
- `snippetstore.{h,cpp}`: fachada en memoria sobre `SnippetRepository` con
  señales de cambio y orden alfabético.

### 4.6 `terminal/`

| Archivo | Responsabilidad |
|---|---|
| `terminalemulator.{h,cpp}` | Modelo de pantalla + parser VT/xterm. Sin widgets, sin procesos. |
| `terminalsession.h` | Interfaz de transporte de bytes. |
| `ptyterminalsession.{h,cpp}` | Transporte PTY local sobre `PtyProcess`. |
| `ptyprocess.{h,cpp}` | `forkpty`, lectura con `QSocketNotifier`, escritura, resize, cierre. |
| `terminalwidget.{h,cpp}` | Vista: pintado, teclado, ratón, IME, selección, búsqueda, scrollback, portapapeles. |

### 4.7 `plugins/`

- `pluginapi.h`: contrato estable (`Plugin`, `PluginHost`, `PluginMetadata`,
  IID, `kPluginApiVersion`).
- `pluginmanager.{h,cpp}`: carga desde rutas conocidas, valida IID y versión,
  aísla fallos por plugin.

### 4.8 `app/` y `ui/`

- `app/applicationcontext.{h,cpp}`: crea base de datos, ajustes, repositorio,
  store, credenciales, registro de comandos y plugins; ejecuta la migración.
- `ui/mainwindow.{h,cpp}`: ventana, menús, toolbar, pestañas, splits,
  operaciones de sesión y pegado de comandos.
- `ui/sidebar.{h,cpp}`: árbol de grupos, grupos virtuales de favoritos y
  recientes, filtro y menú contextual.
- `ui/terminalpane.{h,cpp}`: contenedor de una o varias vistas de terminal.
- `ui/sessiondialog.{h,cpp}`: editor de sesiones, campos avanzados, perfil de
  terminal, port forwards y generación de claves.
- `ui/sshconfigdialog.{h,cpp}`: selección de hosts importables desde
  `~/.ssh/config`, con filtro y detección de duplicados.
- `ui/knownhostsdialog.{h,cpp}`: visor de `known_hosts` con filtro y borrado
  selectivo.
- `ui/sshagentdialog.{h,cpp}`: estado del agente e identidades cargadas, con
  opción de volver a consultar.
- `ui/snippetdialog.{h,cpp}`: catálogo de snippets, editor, filtro y vista
  previa expandida para la sesión activa.
- `ui/globalsearchdialog.{h,cpp}`: búsqueda sobre una instantánea del scrollback
  de todas las terminales abiertas.
- `ui/theme.{h,cpp}`: paletas y hojas de estilo.

---

## 5. Arranque y ciclo de vida

```
main()
 └─ QApplication
    └─ ApplicationContext(AppPaths::system())
       ├─ AppPaths::ensure()
       ├─ Database::open()            → PRAGMAs + migración
       ├─ SettingsStore::initialize()
       ├─ SqliteSessionRepository
       ├─ importLegacyData()          → una sola vez
       ├─ SessionStore::load()
       ├─ SnippetStore::load()
       ├─ CredentialStore            → libsecret o Unavailable
       ├─ CommandRegistry
       └─ PluginManager::loadAll()
    └─ MainWindow(context)
    └─ app.exec()
```

Reglas:

- Los fallos de migración o de plugin son **advertencias**, no abortan el
  arranque; se exponen en `ApplicationContext::warnings()`.
- Un fallo al abrir la base de datos sí es fatal: sin persistencia no hay
  catálogo fiable.
- Los tests construyen el contexto con `AppPaths::forRoot(QTemporaryDir)` y
  jamás tocan `~/.local/share/QTerm2`.

---

## 6. Motor de terminal

### 6.1 Separación

```
 bytes ──► TerminalSession ──dataReceived──► TerminalEmulator ──updated──► TerminalWidget
                                                   │
                                                   └──response──► TerminalSession
 teclado/ratón/IME ────────────────────────────────┴──► TerminalSession::write()
```

- **`TerminalEmulator`**: celdas, cursor, atributos, buffers principal y
  alternativo, scrollback, márgenes, tabs, modos DEC, OSC, UTF-8 incremental.
  Emite `updated`, `screenReset`, `titleChanged`, `bellRang`,
  `workingDirectoryChanged`, `response`, `cursorStyleRequested`.
- **`TerminalSession`**: contrato de transporte.
  ```cpp
  bool start(...); bool startShell(...);
  void write(const QByteArray &); void resize(int cols, int rows);
  void terminate(); bool isRunning() const;
  signals: dataReceived(QByteArray); finished();
  ```
- **`PtyTerminalSession`**: implementación local; el widget la crea por
  defecto.
- **`TerminalWidget`**: única capa que conoce píxeles, fuentes, portapapeles y
  eventos Qt. Mantiene estado **de vista** (offset de scroll, selección,
  búsqueda, parpadeo).

### 6.2 Añadir un transporte nuevo

1. Implementar `qterm2::TerminalSession`.
2. Llamar `terminalWidget->setTransport(new MySession)`; el widget toma
   propiedad y conecta `dataReceived`/`finished`.
3. No tocar el emulador ni el parser.

Esto es lo que permitirá un canal libssh, una consola serie o un contenedor sin
duplicar el motor.

### 6.3 Cobertura VT actual

- C0: BEL, BS, HT, LF/VT/FF, CR.
- CSI: cursor (A/B/C/D/E/F/G/H/f/d), borrado (J/K/X), inserción/borrado de
  líneas y caracteres, scroll (S/T), SGR completo (incluido 256 y truecolor),
  scroll region, origen, autowrap, insert mode, DECSCUSR, DECSTBM, DA/CPR.
- DEC private: cursor visible, cursor de aplicación, bracketed paste, mouse
  1000/1002/1003/1006, focus reporting, alt screen 47/1047/1049.
- OSC: título (0/2) y directorio de trabajo (7). **OSC 52 se ignora a
  propósito** (ver seguridad).
- UTF-8 incremental con anchos de celda para CJK y emoji; combinantes
  descartados de forma segura.

### 6.4 Invariantes del modelo

- Las líneas se reemplazan completas (`TermLine`), nunca se mutan celdas
  compartidas.
- Al reducir filas, las líneas que salen por arriba pasan al scrollback solo en
  la pantalla principal.
- El scrollback respeta `maxHistory` (líneas) y `maxHistoryBytes` (memoria
  aproximada). El segundo límite evita que líneas muy anchas consuman RAM sin
  control; al recortar se ajusta el contador `historyBytes()`.
- La vista no escribe en el modelo: solo lee buffers y llama a `feed()`.

### 6.5 Perfiles de terminal por sesión

`Session::terminalProfile` permite que una sesión tenga fuente, tamaño,
scrollback, forma de cursor y parpadeo propios. `TerminalPane::applyAppearance()`
recibe siempre las preferencias globales y aplica encima los campos definidos
por el perfil; así los splits nuevos heredan la misma apariencia y cambiar las
preferencias globales sigue afectando a las sesiones sin perfil.

El perfil se guarda como JSON en `sessions.terminal_profile` (introducido en el esquema v3). Un
perfil desactivado conserva sus valores para que el usuario pueda reactivarlo
sin volver a configurarlo.

---

## 7. Subsistema SSH

### 7.1 Estrategia

QTerm2 **no reimplementa SSH**. Ejecuta el cliente OpenSSH del sistema dentro
de un PTY, igual que hace el usuario en su terminal. Ventajas:

- respeta `~/.ssh/config`, `known_hosts`, agent, FIDO2, PKCS#11, ProxyJump y
  toda la política local;
- los prompts de contraseña y passphrase ocurren dentro del PTY y nunca pasan
  por la aplicación;
- el comportamiento es idéntico al de `ssh` en consola.

El coste es que la UI no puede inspeccionar el canal. Para SFTP, túneles
gestionados y métricas se añadirá más adelante un backend libssh **como
transporte alternativo**, no como reemplazo obligatorio.

### 7.2 Construcción del comando

`ssh::buildConnectSpec()` traduce `Session` a `ProcessSpec`:

| Sesión | Argumentos |
|---|---|
| Puerto ≠ 22 | `-p <puerto>` |
| Keepalive | `-o ServerAliveInterval=30 -o ServerAliveCountMax=3 -o TCPKeepAlive=yes` |
| Timeout | `-o ConnectTimeout=<n>` |
| Host key `Strict` | `-o StrictHostKeyChecking=yes` |
| Host key `AcceptNew` | `-o StrictHostKeyChecking=accept-new` |
| Clave pública | `-i <ruta> -o IdentitiesOnly=yes -o PreferredAuthentications=publickey` |
| Agent | `-o PreferredAuthentications=publickey` |
| Password | `PubkeyAuthentication=no`, `PreferredAuthentications=password,keyboard-interactive` |
| ProxyJump | `-J <host>` |
| X11 / agent | `-X` / `-A` |
| Port forward local | `-L [listen:]port:host:port` |
| Port forward remoto | `-R [listen:]port:host:port` |
| Port forward dinámico | `-D [listen:]port` |
| Comando inicial | `-t` + comando |
| Destino | `usuario@host` (IPv6 entre corchetes) |

Los argumentos extra del usuario se separan con `QProcess::splitCommand()` y se
insertan como argv, **nunca** se pasan a un shell.

### 7.3 `~/.ssh/config`

`ssh::parseSshConfig()` lee el archivo con un tokenizador que respeta comillas y
comentarios, sigue `Include` con globs y límite de profundidad, y descarta los
bloques con comodines (`*`, `?`, `!`) porque no representan un servidor
concreto. Reconoce `HostName`, `User`, `Port`, `IdentityFile` y `ProxyJump`;
`HostName` admite `%h`.

La exportación no reescribe el archivo completo: `writeManagedSshConfig()`
sustituye solo el bloque entre `# >>> QTerm2 managed block >>>` y
`# <<< QTerm2 managed block <<<`, conserva el resto, crea una copia
`.qterm2.bak-<fecha>` y escribe de forma atómica con permisos `0600`.

### 7.4 `known_hosts`

`ssh::parseKnownHosts()` separa hosts, tipo de clave, material y comentario. Las
entradas con hash (`|1|…`) se marcan y solo pueden eliminarse por número de
línea. `removeKnownHostLines()` conserva comentarios y líneas no seleccionadas,
hace copia de seguridad y reescribe con permisos `0600`.

### 7.5 Reconexión

`Session::autoReconnect` es opcional y se configura por sesión. Cuando el
proceso SSH termina, `MainWindow` programa hasta 5 reintentos con backoff
exponencial (1, 2, 4, 8 y 16 s). Cada reintento limpia el emulador y relanza el
`ProcessSpec` sobre el mismo transporte PTY. Solo se considera estable una
conexión que sigue viva tras 30 s; entonces se reinicia el contador. Un cierre
de ventana, pestaña o panel cancela los temporizadores.

### 7.6 Agente SSH

`ssh::queryAgent()` comprueba `SSH_AUTH_SOCK` y ejecuta `ssh-add -l` sin shell ni
TTY. El resultado se interpreta como una lista de identidades (bits, huella,
tipo y comentario); los códigos de salida 0 (con identidades) y 1 (agente vacío)
se consideran disponibles, y el 2 se traduce a un error legible. QTerm2 **no**
carga, desbloquea ni elimina llaves: eso queda en `ssh-add`, que es quien puede
pedir la passphrase o hablar con un token hardware. La UI es un diálogo de solo
lectura con opción de refrescar.

### 7.7 Claves
`ssh::KeyManager` delega en `ssh-keygen` y `ssh-copy-id`:

- generar Ed25519 con comentario y passphrase opcional;
- negarse a sobrescribir una clave existente;
- comprobar permisos de la clave privada;
- obtener la clave pública para mostrarla o instalarla.

La passphrase se pasa a `ssh-keygen` sin escribirla en disco; no se persiste.

---

## 8. Persistencia

### 8.1 Rutas

| Uso | Ruta |
|---|---|
| Configuración no secreta | `~/.config/QTerm2/` |
| Datos durables | `~/.local/share/QTerm2/qterm2.db` |
| Caché desechable | `~/.cache/QTerm2/` |

### 8.2 Esquema v4

```sql
CREATE TABLE groups (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL UNIQUE,
    parent_id   TEXT REFERENCES groups(id) ON DELETE SET NULL,
    sort_order  INTEGER NOT NULL DEFAULT 0,
    created_at  TEXT NOT NULL
);

CREATE TABLE sessions (
    id                TEXT PRIMARY KEY,
    name              TEXT NOT NULL,
    group_name        TEXT NOT NULL DEFAULT 'Servidores',
    type              TEXT NOT NULL DEFAULT 'ssh',          -- local | ssh
    host              TEXT NOT NULL DEFAULT '',
    port              INTEGER NOT NULL DEFAULT 22,
    username          TEXT NOT NULL DEFAULT '',
    auth_method       TEXT NOT NULL DEFAULT 'publickey',    -- publickey | agent | password
    private_key       TEXT NOT NULL DEFAULT '',
    proxy_jump        TEXT NOT NULL DEFAULT '',
    extra_args        TEXT NOT NULL DEFAULT '',
    start_dir         TEXT NOT NULL DEFAULT '',
    initial_command   TEXT NOT NULL DEFAULT '',
    shell             TEXT NOT NULL DEFAULT '',
    x11_forwarding    INTEGER NOT NULL DEFAULT 0,
    forward_agent     INTEGER NOT NULL DEFAULT 0,
    keep_alive        INTEGER NOT NULL DEFAULT 1,
    auto_reconnect    INTEGER NOT NULL DEFAULT 0,          -- añadido en v2
    terminal_profile  TEXT NOT NULL DEFAULT '{}',           -- JSON, añadido en v3
    connect_timeout   INTEGER NOT NULL DEFAULT 10,
    host_key_policy   TEXT NOT NULL DEFAULT 'ask',          -- ask | strict | accept-new
    port_forwards     TEXT NOT NULL DEFAULT '[]',           -- JSON
    favorite          INTEGER NOT NULL DEFAULT 0,
    last_used_at      TEXT,
    created_at        TEXT NOT NULL,
    updated_at        TEXT NOT NULL,
    color_tag         TEXT NOT NULL DEFAULT 'blue',
    notes             TEXT NOT NULL DEFAULT ''
);

CREATE INDEX idx_sessions_group ON sessions(group_name);

CREATE TABLE settings (
    key         TEXT PRIMARY KEY,
    value       BLOB NOT NULL,
    updated_at  TEXT NOT NULL
);

CREATE TABLE snippets (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    command     TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL
);
```

Notas:

- `port_forwards` es JSON porque es una lista pequeña y siempre se lee junto a
  la sesión; si en el futuro se consultan por separado, se normalizará.
- `groups` existe para jerarquía futura; hoy la UI usa `group_name`. La
  migración a `group_id` será una versión de esquema nueva.
- Todas las columnas `TEXT` son `NOT NULL`; el repositorio convierte `QString`
  nulos en `""` para no violar la restricción.
- `PRAGMA user_version` es la única fuente de verdad de la versión.
- **v2** añade `auto_reconnect` con `ALTER TABLE sessions ADD COLUMN` y valor
  por defecto `0`, de modo que las sesiones existentes conservan el
  comportamiento anterior (sin reconexión).
- **v3** añade `terminal_profile` como JSON (`{enabled, fontFamily, fontSize,
  scrollbackLines, cursorStyle, cursorBlink}`). El valor por defecto `{}` deja
  la sesión con la apariencia global; un perfil activo solo sustituye los
  campos que define.
- **v4** añade la tabla `snippets`. El comando se guarda como plantilla sin
  expandir; la sustitución de `${host}`, `${user}`, `${port}`, `${name}` y
  `${group}` se hace en memoria para la sesión activa.

### 8.3 Migraciones

`Database::migrate()` es transaccional y secuencial:

1. lee `user_version`;
2. rechaza la base si es más nueva que `kSchemaVersion`;
3. aplica los bloques pendientes en orden (`if (version < N) { ...; version = N; }`);
4. verifica que la versión local sea exactamente `kSchemaVersion`;
5. escribe `PRAGMA user_version` y hace commit;
6. si algo falla, hace rollback.

Los bloques ya publicados **no se modifican**: cada cambio de esquema añade un
bloque nuevo y sube `kSchemaVersion`. Así una base creada por una versión
antigua y otra creada desde cero convergen al mismo esquema.

Si la base de datos es **más nueva** que el binario, se rechaza abrirla con un
mensaje claro. Nunca se "adivina" el esquema.

### 8.4 Migración legada

`importLegacyData()`:

- importa `sessions.json` y `QTerm2.conf` una sola vez;
- escribe primero los datos y después la marca de migración;
- renombra los archivos originales a `*.migrated-YYYYMMDD-HHmmss`;
- es idempotente: una segunda ejecución no duplica sesiones.

### 8.5 Ajustes

`SettingsStore` guarda `QVariant` serializado en BLOB. La UI usa claves
namespaced (`terminal/fontSize`, `ui/darkTheme`, `startup/openLocalTab`…). No
hay `QSettings` en runtime; solo se usa para leer el archivo legado.

---

## 9. Seguridad

### 9.1 Modelo de amenazas

| Amenaza | Mitigación |
|---|---|
| Robo de contraseñas/passphrases desde disco | Nunca se persisten. Solo Secret Service si el usuario lo pide. |
| Secreto en logs o volcados | No se registran secretos ni entornos con credenciales. |
| Inyección de comandos por nombre de host/usuario | `ProcessSpec` usa argv; no hay `sh -c`. |
| Host key desconocida aceptada a ciegas | Política explícita por sesión; por defecto `ask` (OpenSSH decide). |
| Programa remoto escribe el portapapeles | OSC 52 ignorado. |
| Plugin malicioso o incompatible | IID + versión + inicialización aislada; superficie mínima. |
| Clave privada legible por terceros | `KeyManager` comprueba permisos; `ssh-keygen` crea `0600`. |
| Registro de credenciales por `libsecret` no disponible | Backend `Unavailable` que falla de forma explícita; la app sigue funcionando. |

### 9.2 Reglas para secretos

1. `Session` no tiene campos de contraseña ni passphrase.
2. SQLite/JSON nunca contienen secretos.
3. Si en el futuro se ofrece "recordar contraseña", se usará
   `CredentialStore` con la clave `CredentialKey::sessionPassword(id)`; la UI
   solo recibirá el secreto en memoria y lo inyectará en el PTY cuando
   corresponda.
4. La passphrase de una clave se pasa a `ssh-keygen` por entrada estándar o
   argumento efímero, nunca por línea de comandos visible en `ps` cuando se
   pueda evitar.

### 9.3 `extraArgs`

Es una válvula de escape deliberada para usuarios avanzados. Se parsea con
`QProcess::splitCommand()` y se pasa como argv a `ssh`. No se ejecuta en un
shell, pero puede alterar el comportamiento de SSH (por ejemplo
`-o ProxyCommand=...`). La UI debe advertirlo.

### 9.4 Plugins

Un plugin recibe `PluginHost`:

```cpp
class PluginHost {
public:
    virtual int apiVersion() const = 0;
    virtual CommandRegistry *commands() const = 0;
    virtual SessionRepository *sessions() const = 0;
};
```

No recibe widgets, ni `QProcess`, ni acceso a credenciales en v1. Esto limita
el daño de un plugin defectuoso o malicioso. Las capacidades nuevas se añaden
en `v2` con interfaces separadas y revisión explícita.

---

## 10. Sistema de plugins

### 10.1 Contrato

```cpp
class Plugin {
public:
    virtual PluginMetadata metadata() const = 0;
    virtual bool initialize(PluginHost *host, QString *errorMessage) = 0;
    virtual void shutdown() = 0;
};

#define QTerm2Plugin_iid "org.qterm2.QTerm2Plugin/1.0"
```

`PluginMetadata` incluye `id`, `name`, `version`, `vendor`, `description` y
`apiVersion`.

### 10.2 Carga

Rutas, en orden:

1. `<dir-del-ejecutable>/plugins`
2. `<prefix>/lib/qterm2/plugins`
3. `~/.local/lib/qterm2/plugins`

Solo se consideran `.so`/`.dylib`. Un plugin se rechaza si:

- no implementa el IID;
- `metadata().apiVersion != kPluginApiVersion`;
- `metadata().id` está vacío o duplicado;
- `initialize()` devuelve `false`.

El resto de plugins sigue cargando. Los errores se acumulan y se exponen como
advertencias.

### 10.3 Ejemplo mínimo

```cpp
class HelloPlugin final : public QObject, public qterm2::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QTerm2Plugin_iid FILE "hello.json")
    Q_INTERFACES(qterm2::Plugin)
public:
    qterm2::PluginMetadata metadata() const override
    {
        return { "hello", "Hello", "1.0", "ACME", "Ejemplo", 1 };
    }
    bool initialize(qterm2::PluginHost *host, QString *) override
    {
        qterm2::Command c;
        c.id = "hello.say";
        c.title = "Decir hola";
        c.category = "Plugins";
        c.pluginId = "hello";
        c.action = [] { /* ... */ };
        return host->commands()->registerCommand(c);
    }
    void shutdown() override {}
};
```

### 10.4 Compatibilidad

- Añadir un método a `PluginHost` **rompe** la ABI aunque sea virtual puro.
- Por eso v2 será una **nueva interfaz** (`PluginHostV2`) con su propio IID o
  una versión de API distinta.
- Los comandos de un plugin se eliminan al descargarlo
  (`unregisterCommand` por `pluginId`).

---

## 11. UI

### 11.1 Composición

```
MainWindow
├─ QSplitter horizontal
│  ├─ Sidebar            (grupos, filtro, favoritos, menú contextual)
│  └─ QWidget
│     ├─ QTabWidget      (pestañas)
│     │  └─ TerminalPane (splits recursivos)
│     │     └─ TerminalWidget + barra de búsqueda
│     └─ barra de estado
├─ menús: Archivo, Editar, Sesiones, Terminal, Ver, Ayuda
└─ toolbar
```

### 11.2 Reglas

- `MainWindow` no abre SQL ni `QSettings`; usa `ApplicationContext`,
  `SessionStore`, `SettingsStore` y `SessionLauncher`.
- `Sidebar` no conoce SSH; emite señales (`connectRequested`, `editRequested`,
  `favoriteToggleRequested`, `duplicateRequested`, `deleteRequested`,
  `installKeyRequested`, `copyCommandRequested`).
- Los grupos `Favoritos` y `Recientes` son **virtuales**: no son filas de la
  tabla `groups`. Una sesión favorita o reciente sigue apareciendo también en
  su grupo real. `Recientes` ordena por `lastUsedAt` descendente y muestra como
  máximo 8 entradas. `MainWindow::toggleFavorite` persiste el cambio con
  `SessionStore::updateSession`.
- `SessionDialog` edita todos los campos persistibles: favorito, política de
  host key, timeout, reconexión, perfil de terminal y una tabla de port
  forwards con columnas por fila. Las
  filas desactivadas se guardan sin validar el destino; las activas exigen host
  y puerto destino salvo en los forwards dinámicos.
- `TerminalPane` encapsula splits y búsqueda; `MainWindow` no manipula
  `TerminalWidget` directamente salvo para ajustes globales.
- Los atajos y acciones se registran como comandos cuando son globales; así la
  Command Palette y los plugins comparten el mismo catálogo (`qterm2.<actionKey>`).
- La forma y el parpadeo del cursor se persisten como
  `terminal/cursorStyle` (`block`, `ibeam`, `underscore`) y
  `terminal/cursorBlink` (bool). El estilo configurado es el valor por defecto
  y se restaura cuando la aplicación envía `DECSCUSR 0`; el parpadeo
  configurado es el interruptor maestro, de modo que `DECSCUSR` no puede
  reactivarlo si el usuario lo desactivó.
- El cursor I-Beam se dibuja con `U+258F LEFT ONE EIGHTH BLOCK`, alineado al
  borde izquierdo de la celda y con la altura completa de esta. Se desactiva el
  antialiasing de texto al pintarlo para que el trazo sea sólido y no aparezca
  con flecos de color; no se usa `|` porque su glifo queda centrado en el
  ancho de avance y depende de la fuente.

### 11.3 Command Palette

`Ctrl+Shift+P` abre `CommandPaletteDialog`, un `QDialog` con `QLineEdit` y
`QListWidget`. `MainWindow::registerCommands()` registra cada `QAction` como
`qterm2.<actionKey>` con título, categoría y atajo; la palette solo muestra los
comandos `enabled` y ejecuta el elegido a través de `CommandRegistry`, no del
widget. Así los comandos nativos y los de plugins siguen el mismo camino.

El filtro divide el texto en tokens y exige que **cada token** aparezca en
alguno de los campos (título, categoría, id o atajo), sin distinguir mayúsculas.
Cuando no hay coincidencias, el pie lo explica en lugar de ejecutar el primer
elemento. El registro se puede volver a construir sin duplicados: antes de
re-registrar, `MainWindow` elimina los ids que añadió la vez anterior.

### 11.4 Snippets

`Ctrl+Shift+S` abre `SnippetDialog`. El catálogo vive en la tabla `snippets`
(esquema v4) y se edita en el mismo diálogo. El comando se guarda **sin
expandir**; al pulsar «Insertar en la terminal», `expandSnippet()` sustituye
`${host}`, `${user}`, `${port}`, `${name}` y `${group}` con los datos de la
sesión activa y `MainWindow` escribe el resultado con `TerminalWidget::sendText`
sin añadir un salto de línea. El snippet nunca se ejecuta solo: el usuario ve el
texto en la línea de comandos y decide cuándo pulsar Enter.

`$$` produce un dólar literal y los placeholders desconocidos se conservan para
que un error de escritura sea visible. En una sesión local, `${host}` cae a
`localhost`, `${user}` al usuario del entorno y `${port}` a 22.

### 11.5 Búsqueda global

`Ctrl+Shift+G` abre `GlobalSearchDialog` con una **instantánea** de
`TerminalWidget::screenText()` por cada terminal (incluidos los paneles de un
split). La búsqueda es incremental, sin distinguir mayúsculas, y se limita a
1000 resultados para no bloquear la UI. Cada resultado guarda el índice de
terminal y la línea absoluta; al aceptar, `MainWindow` cambia de pestaña,
activa el panel concreto, marca el texto con `setSearchText()` y desplaza la
vista con `scrollToLine()`. La instantánea evita recorrer modelos vivos mientras
los PTY siguen escribiendo; si la salida cambia entre la búsqueda y el salto, la
línea puede haberse desplazado y el usuario puede repetir la búsqueda.

---

## 12. Build, pruebas y diagnóstico

### 12.1 Dependencias

- CMake ≥ 3.16, C++17;
- Qt 6.2+: `Core`, `Gui`, `Widgets`, `Sql` (driver `QSQLITE`);
- POSIX (`forkpty`, `util`) en Linux/macOS;
- `openssh-client` en runtime;
- `libsecret-1` opcional en compilación.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

### 12.2 Suites

| Test | Cobertura |
|---|---|
| `smoketest` | Sesiones, `SessionStore` sobre SQLite, PTY, parser vía widget, zsh interactivo, claves Ed25519. |
| `uitest` | `MainWindow` headless, sidebar, pestañas, tema, terminal local y screenshot opcional. |
| `archtest` | SSH builder, DB/ajustes, repositorio, migración legada, credenciales, `CommandRegistry`, emulador sin widget, `ApplicationContext`. |

Variables útiles:

- `QT_QPA_PLATFORM=offscreen` para CI;
- `QTERM2_SCREENSHOT=/ruta.png` captura la ventana;
- `QTERM2_TEST_DUMP=1` imprime el contenido de pantalla.

### 12.3 Diagnóstico

- Advertencias de arranque: `ApplicationContext::warnings()`.
- Errores de SQL: se propagan como `QString *errorMessage`, no se silencian.
- Plugins rechazados: `PluginManager::errors()`.

---

## 13. Hoja de ruta

### Fase 1 — base sólida (actual)

- [x] Separación core/UI/terminal/SSH/persistencia.
- [x] SQLite + migración legada.
- [x] Transporte PTY + emulador independiente.
- [x] API de plugins v1.
- [x] Tests de arquitectura.

### Fase 2 — gestión SSH completa

- [x] Editor de port forwards, timeout y política de host key.
- [x] Favoritos y recientes visibles en la sidebar.
- [x] Importación/exportación de `~/.ssh/config` en bloque gestionado.
- [x] Vista de `known_hosts` y eliminación de entradas.
- [x] Reconexión automática opcional con backoff por sesión.
- [x] Perfiles de terminal por sesión.
- [x] Integración explícita con SSH Agent (`SSH_AUTH_SOCK`).

### Fase 3 — SFTP y túneles gestionados

- [x] `SftpSession` como servicio separado (libssh2 o `sftp` interactivo).
- [x] Panel de transferencias con cola, progreso y reanudación.
- [x] Editor remoto seguro (descarga temporal, edición, subida).
- [ ] Túneles `-L/-R/-D` gestionados desde UI con estado y reconexión.
- [x] Backend `LibsshTerminalSession` opcional para canales nativos.

### Fase 4 — extensibilidad

- [x] Command Palette.
- [x] Snippets con variables (`${host}`, `${user}`).
- [x] Historial de comandos por sesión.
- [x] API de plugins v2 (paneles, temas, backends) con revisión de permisos.
- [x] Integración Docker/Kubernetes como plugins.

### Fase 5 — producto

- [x] Búsqueda global en scrollback de todas las pestañas.
- [x] Sesiones sincronizadas (exportar/importar cifrado).
- [x] Telemetría **opt-in** y local.
- [x] Empaquetado Flatpak/AppImage y firma.

---

## 14. Deuda técnica conocida

1. **`TerminalWidget` sigue siendo grande** (~1.100 líneas) aunque ya no contiene
   el parser. Próximo paso: extraer selección/scrollback y búsqueda a
   colaboradores (`TerminalSelection`, `TerminalSearch`) para dejar la vista en
   pintado e input.
2. **Los perfiles de terminal no incluyen colores propios**: solo fuente,
   tamaño, scrollback, forma de cursor y parpadeo. Un esquema de color por
   sesión requeriría ampliar `TerminalProfile` y `Theme`.
3. **`groups` no se usa todavía**; la UI agrupa por `group_name` y deriva
   `Favoritos`/`Recientes` en memoria.
4. **`LibSecretCredentialStore::isAvailable()`** crea una conexión D-Bus síncrona
   cada vez que se consulta; debería cachearse.
5. **El parser no soporta secuencias OSC 8 (hipervínculos)** ni Sixel; los
   enlaces actuales se detectan por texto.
6. **El contador de memoria del scrollback es aproximado** (`sizeof` de línea y
   celda), suficiente como cota de seguridad pero no como medición exacta del
   allocator.
7. **La reconexión automática no distingue** entre caída de red y salida
   voluntaria del usuario; un `exit` limpio también dispara reintentos si la
   sesión tiene la opción activa.
8. **La migración no valida el JSON legado con un esquema formal**; tolera
   campos ausentes, pero conviene un informe de entradas descartadas.

---

## 15. Decisiones de arquitectura (resumen)

| # | Decisión | Motivo |
|---|---|---|
| ADR-01 | Qt 6 Widgets + C++17 | Madurez, rendimiento y control fino del terminal. |
| ADR-02 | SQLite para metadata y ajustes | Transacciones, consultas, un solo almacén y migraciones versionadas. |
| ADR-03 | OpenSSH del sistema en PTY | Compatibilidad total con configuración, agentes y hardware keys. |
| ADR-04 | Parser VT propio | Control de rendimiento y de comportamientos específicos de terminal. |
| ADR-05 | Secretos fuera de la base de datos | Reduce el impacto de una filtración. |
| ADR-06 | `ProcessSpec` con argv | Evita inyección de shell. |
| ADR-07 | `ApplicationContext` como composition root | Tests herméticos y UI sin construcción de servicios. |
| ADR-08 | Plugins v1 con superficie mínima | Seguridad y estabilidad antes que potencia. |
| ADR-09 | Emulador separado de la vista | Testabilidad y futuros transportes (libssh, serial, contenedores). |

---

## 16. Reglas para contribuir

1. No añadir `#include` de UI en `core`, `domain`, `storage`, `security`,
   `ssh` o `terminal` (salvo `terminalwidget`, que es UI).
2. No abrir SQLite desde la UI; usar repositorios.
3. No guardar secretos en `Session`, SQLite, JSON ni logs.
4. Todo proceso nuevo se describe con `ProcessSpec` y se lanza sin shell.
5. Todo cambio de esquema incrementa `kSchemaVersion` y añade un paso de
   migración transaccional.
6. Todo comando global se registra en `CommandRegistry` si debe ser accesible
   desde palette o plugins.
7. Todo transporte nuevo implementa `TerminalSession`; no se modifica el
   emulador para acomodarlo.
8. Todo cambio de comportamiento relevante añade o actualiza un test en
   `archtest`, `smoketest` o `uitest`.
