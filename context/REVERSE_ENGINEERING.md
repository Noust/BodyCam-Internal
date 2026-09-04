# Bodycam — Ingeniería inversa (UE5)

> **Fuente de verdad de offsets de este proyecto.**
> Cada dato lleva su evidencia: `[REFL]` = reflexión `UECodeGen_Private::FPropertyParams`,
> `[ASM]` = desensamblado, `[XVAL]` = validación cruzada contra `sizeof` de la clase,
> `[DED]` = deducido (NO verificado — tratar como suposición).
> Si un dato no tiene evidencia, no está verificado. No lo uses como hecho.

---

## 0. TL;DR — lo que hay que saber antes de tocar nada

> **Build vigente: la del parche del 2026-09-03.** 394.579 funciones. La anterior
> tenía 394.552 y **todos los RVAs cambiaron**. Los offsets de clase **no**: ver §12.

| Cosa | Valor | Nota |
|---|---|---|
| Proceso | `Bodycam-Win64-Shipping.exe` | ImageBase `0x140000000` |
| Módulo del juego | `/Script/Bodycam` | 156 clases propias |
| Arquitectura del cheat | **DLL inyectada** | lectura in-process, overlay D3D9 propio |
| `GWorld` | **se busca y valida en runtime** | no hay RVA fiable — ver §2.1 |
| Clase del jugador | `ABodycamCharacter` (`0x680`) | deriva de `ACharacter` (`0x650`) |
| Vida | GAS: `Character+0x668 → UCharacterAttributeSet+0x94` | **float**, no double |
| Equipo | `APlayerState+0x388` (`TeamId`, int32) | NO está en el pawn |
| Roster | `AGameStateBase+0x2C0` (`PlayerArray`) | el código viejo decía `0x338` ✗ |

### Las seis trampas de este juego

**A) El `PlayerArray` viejo (`0x338`) es imposible.**
`AGameStateBase` mide `0x300` `[XVAL]`. Un offset `0x338` cae fuera de la clase.
El valor real es **`0x2C0`** `[REFL]`, y es un `TArray<APlayerState*>`:
`Data = *(void**)(gs+0x2C0)`, `Num = *(int32*)(gs+0x2C8)`, **stride 8**.
El código viejo usaba stride `0x318`, que no corresponde a nada.

**B) `POV.FOV` no siempre es horizontal.**
El eje que se conserva lo decide `ULocalPlayer::AspectRatioAxisConstraint@0xB8` `[REFL]`.
Usar `width/2` en ambos ejes (como hacía `WorldToScreen.hpp`) desplaza las cajas
**hacia afuera** del centro, cada vez más cerca del borde (factor W/H = 1.78 en 16:9).
Fórmula correcta en §5.

**C) La vida ya no es un `double`.**
El juego usa **GameplayAbilities (GAS)**. `Health` es un `FGameplayAttributeData`
(16 B: `vtable@0x00`, `BaseValue@0x08`, **`CurrentValue@0x0C`**) `[REFL]`.
El código viejo leía un `double` en `+0xB0`; hoy es un `float` en `+0x94`.

**D) `w2s` debe descartar lo que está detrás de la cámara.**
Hacer `if (z < 1) z = 1` en vez de rechazar el punto dibuja a los enemigos de
detrás como si estuvieran delante, en espejo. Hay que devolver "no visible".

**E) `GWorld` no se puede derivar por RVA con heurísticas de lecturas/escrituras.**
Ese perfil lo cumple `__security_cookie`. Ver §2.1.

**F) Validar el `UWorld` por su `GameInstance` NO sirve: da un falso positivo eterno.**
`UGameInstance` y `ULocalPlayer` **sobreviven al cambio de mapa**. Así que un
`UWorld` viejo y ya destruido sigue teniendo un `+0x1D8` que resuelve, y con él
todo el resto de la cadena hasta el `PlayerController`. Un ancla que apunte a ese
mundo muerto **pasa la validación para siempre**: el `GameState` que se lee es el
del mapa anterior, el roster sale vacío y nunca se vuelve a buscar.
La prueba correcta es específica del mundo: **el `PlayerArray` del `GameState` de
ese `UWorld` tiene que contener tu propio `APlayerState`.** Ver §2.1.

---

## 1. Entorno

```
Ruta      D:\SteamLibrary\steamapps\common\Bodycam\Bodycam\Binaries\Win64\Bodycam-Win64-Shipping.exe
ImageBase 0x140000000
Funciones 394.579        (build del 2026-09-03; la anterior tenía 394.552)
```

### Segmentos (build 2026-09-03)

| Nombre | Inicio | Fin | Tamaño |
|---|---|---|---|
| `.text`   | `0x140001000` | `0x14764b000` | `0x764a000` |
| `.idata`  | `0x14764b000` | `0x14764d4c0` | `0x24c0` |
| `.rdata`  | `0x14764d4c0` | `0x14976b000` | `0x211db40` |
| `.data`   | `0x14976b000` | `0x14996ffe8` | `0x204fe8` |
| `.idata` (2) | `0x14996ffe8` | `0x149971060` | `0x1078` |
| `.data` (2) | `0x149971060` | `0x149daa000` | `0x438fa0` |
| `.pdata`  | `0x149daa000` | `0x14a3a7000` | `0x5fd000` |
| `_RDATA`  | `0x14a3a9000` | `0x14a407000` | `0x5e000` |

`.text` creció `0x1000` respecto a la build anterior, así que **las direcciones de
código no se desplazaron todas por igual** y las de datos sí, pero por `0x1180`,
no por `0x1000`. Moraleja: tras un parche, **no restes deltas: vuelve a buscar**.

### Versión del motor
No hay string `++UE5+Release-x.y` en el binario. La versión se deduce por los
módulos presentes (`/Script/IrisCore`, `/Script/UniversalObjectLocator`,
`/Script/DataflowCore`, `/Script/NNEDenoiser`) y por los layouts observados:
**UE 5.4/5.5** `[DED]`. No importa para el trabajo: **todos los offsets de este
documento se derivaron de ESTE binario**, no de la versión del motor.

Diferencias de layout frente a Hell Let Loose: Vietnam (UE 5.5.x), por si se
copia código entre proyectos — **no son intercambiables**:

| | Bodycam | HLL:V |
|---|---|---|
| `AActor::RootComponent` | `0x1B8` | `0x1C0` |
| `ACharacter::Mesh` | `0x328` | `0x330` |
| `ACharacter::CapsuleComponent` | `0x338` | `0x340` |
| `APawn::PlayerState` | `0x2C8` | `0x2D0` |
| `AController::ControlRotation` | `0x320` | `0x328` |
| `USceneComponent::RelativeLocation` | `0x128` | `0x130` |

Lo que **sí** coincide (y por eso conviene reusar la lógica, no los números):
`ComponentToWorld@0x1D0`, todo `USkinnedMeshComponent` (`0x528`/`0x530`/`0x598`/`0x5E0`),
`UCapsuleComponent` (`0x508`/`0x50C`), `FMinimalViewInfo` completo,
`ULocalPlayer::AspectRatioAxisConstraint@0xB8`, `APlayerCameraManager::CameraCachePrivate@0x1410`.

---

## 2. Globales

> ⚠️ **`GWorld` NO se localiza por RVA en este proyecto.** Ver §2.1: dos intentos
> de derivarlo estáticamente dieron globales equivocadas. Se busca en runtime.

| Global | RVA (build 2026-09-03) | RVA anterior | Evidencia |
|---|---|---|---|
| **`GNames`** (`FNamePool`) | **`0x99C29C0`** | `0x99C1840` | `[ASM]` 11/11 sitios coinciden — ver §2.2 |
| **`GUObjectArray`** (base) | **`0x9AA6160`** | `0x9AA4FE0` | `[ASM]` = `Objects` − `0x10` |
| **`GUObjectArray.ObjObjects.Objects`** | **`0x9AA6170`** | `0x9AA4FF0` | `[ASM]` 814/814 sitios coinciden |
| **`GUObjectArray.ObjObjects.NumElements`** | **`0x9AA6184`** | `0x9AA5004` | `[ASM]` = `Objects` + `0x14` |
| `FNamePool` "ya inicializado" (bool) | `0x99C2767` | — | `[ASM]` flag de la init perezosa |
| `GetPrivateStaticClassBody` | `0x1378880` | `0x1378740` | `[ASM]` 6.122 sitios |
| `__security_cookie` | `0x9934600` (build vieja) | — | `[ASM]` **no es de engine**. Trampa E |

> ⚠️ **Ningún RVA de esta tabla debe ir a fuego en el cliente.** Los tres que
> importan (`FNamePool` y las dos `AddXInput`) se localizan **por firma de bytes
> en runtime**, con el RVA de arriba solo como pista para el camino rápido.
> Ver §2.3.

> ⚠️ El código heredado usaba `GWorld = 0x93CFB50`. Ese RVA es de una build
> anterior y **ya no es válido**.

### GUObjectArray — evidencia
El patrón aparece 814 veces, y **las 814 apuntan a la misma global**:
```asm
mov   eax, [rdi+0Ch]                  ; UObject::InternalIndex
cmp   eax, cs:qword_149AA6184         ; NumElements
jge   short (fuera de rango)
mov   ecx, eax
movzx eax, ax                         ; idx & 0xFFFF
shr   rcx, 10h                        ; idx >> 16  -> indice de chunk
lea   rdx, [rax+rax*2]                ; (idx & 0xFFFF) * 3
mov   rax, cs:qword_149AA6170         ; Objects
mov   rcx, [rax+rcx*8]                ; chunk = Objects[idx >> 16]
lea   rbx, [rcx+rdx*8]                ; item = chunk + (idx & 0xFFFF) * 24
mov   eax, [rbx+8]                    ; FUObjectItem::Flags
```
Firma de bytes para volver a localizarlo tras un parche (0 falsos positivos):
```
48 8D 14 40  48 8B 05 <rel32>  48 8B 0C C8
```
El `rel32` del `mov rax, cs:...` resuelve a `Objects`; `NumElements` está en
`Objects + 0x14` y la base de `FUObjectArray` en `Objects − 0x10`.
De aquí salen, todos `[ASM]`:
- **Chunk de 65536** objetos (`>> 16`), **stride de `FUObjectItem` = `0x18`** (`*3` luego `*8`).
- `FUObjectItem`: `Object @0x00`, `Flags @0x08`.
- **`UObject::InternalIndex = 0x0C`** (deja de ser una suposición).
- `FChunkedFixedUObjectArray`: `Objects @0x00`, `NumElements @0x14`
  (`0x149AA5004 − 0x149AA4FF0 = 0x14`), y `FUObjectArray::ObjObjects @0x10`.

Recorrido:
```
num  = *(int32*)(base + 0x9AA6184)                  // acotar a [0, 5.000.000]
item = (*(uintptr**)(base + 0x9AA6170))[i >> 16] + (i & 0xFFFF) * 0x18
obj  = *(uintptr*)item
flags= *(uint32*)(item + 8)                          // descartar los marcados
```

### 2.2 `GNames` (`FNamePool`) — `0x99C29C0` `[ASM]`

Evidencia, en la resolución de `FName` a texto (`sub_14128B7F0` entre otras):
```asm
cmp   cs:byte_1499C2767, 0     ; ¿pool ya inicializado?
jz    short init
lea   r8, stru_1499C29C0       ; el pool
jmp   short cont
init:
lea   rcx, stru_1499C29C0      ; el mismo pool
call  sub_14127BCD0            ; init perezosa
mov   r8, rax
mov   cs:byte_1499C2767, 1
cont:
mov   edx, ebx                 ; idx
movzx eax, bx                  ; idx & 0xFFFF
shr   edx, 10h                 ; block = idx >> 16
lea   ecx, [rax+rax]           ; offset * 2
add   rcx, [r8+rdx*8+10h]      ; + Blocks[block]     -> Blocks en +0x10
movzx ebx, word ptr [rcx]      ; header
shr   ebx, 6                   ; len = header >> 6
```

De aquí:
- **`Blocks` en `+0x10`** del pool; `Blocks[block]` con `block = idx >> 16`.
- **`entry = Blocks[block] + 2 * (idx & 0xFFFF)`**.
- `FNameEntry`: header `uint16` en `+0x00`, **`len = header >> 6`**, `wide = header & 1`,
  y los caracteres a partir de `+0x02`.

> Cuidado al buscarlo: el patrón "`>>16` + índice `*8` + `lea` con `*2`" lo cumple
> también `GUObjectArray`, cuyo `lea rdx,[rax+rax*2]` tiene el mismo `scale=1` en
> el SIB. Se distinguen porque en `GUObjectArray` **índice y base son el mismo
> registro** (es un `*3`), y en `FNamePool` son distintos (es un `*2` real).

**Para qué se usa aquí:** nombres de hueso (`FMeshBoneInfo::Name@0x00`) para
dibujar solo el esqueleto del cuerpo (§6), y nombre de clase de un pawn para
distinguir jugador de dron (§7).

> ⚠️ **Si `GNames` apunta mal, el síntoma NO es "no hay nombres": es un esqueleto
> roto.** Los nombres salen basura, el filtro por nombre se cae al respaldo, y
> sobre todo **los drones dejan de detectarse**, así que se les lee la pose como
> si fueran personas y salen líneas disparadas. Fue exactamente lo que pasó con
> el parche del 2026-09-03.

### 2.3 Localización por firma en runtime (a prueba de parches)

El parche del 2026-09-03 movió todo el código y todos los datos. Lo único que
sobrevivió sin tocar nada fue `GWorld`, porque se buscaba en runtime. `GNames` y
las funciones del aimbot estaban a fuego y se rompieron. **Ahora las tres se
localizan por firma**, con el RVA conocido solo como camino rápido:

1. Se prueba el RVA conocido y se **verifica**. Si cuadra, no se escanea nada.
2. Si no cuadra, se recorre `.text` en bloques de 64 KB (con 64 B de solape para
   no perder una firma partida entre bloques) buscando el patrón.
3. Si tampoco aparece, se degrada con elegancia: sin nombres el ESP sigue
   funcionando (esqueleto sin filtrar), y el aimbot cae a escribir
   `RotationInput` directamente.

**Firma de `FNamePool`** — 11 sitios en el binario, y en los 11 los dos `lea`
resuelven a la misma dirección, así que la propia firma se autovalida:
```
74 09                 jz +9
4C 8D 05 <rel32>      lea r8,  FNamePool
EB 16                 jmp +0x16
48 8D 0D <rel32>      lea rcx, FNamePool     <- debe dar el MISMO destino
E8 <rel32>            call InitPool
4C 8B C0              mov r8, rax
C6 05 <rel32> 01      mov cs:bInitialized, 1
```
Comprobación funcional adicional: con el pool correcto, el `FName` de índice `0`
resuelve a la cadena `"None"`.

**Firma de `AddPitchInput` / `AddYawInput`** — la cola de la función en `+0x73`,
con el offset del `FRotator` incrustado dos veces (por eso distingue el eje).
Exactamente 3 coincidencias en todo el binario, una por eje. Ver §9.1.

### 2.1 `GWorld`: por qué se busca en runtime y no por RVA

**Dos derivaciones estáticas fallaron. Merece la pena entender por qué, para no
repetirlo.**

**Intento 1 — `0x9934600`.** Se buscó la global con el perfil "muchísimas
lecturas, una sola escritura, ningún `lea`" y salió con 29.689 / 1 / 0. Parecía
concluyente. **Era `__security_cookie`**: el 100 % de esas lecturas van seguidas
de `xor` con `rsp`. El cookie se carga en el prólogo de casi cualquier función
con protección de pila, así que ese perfil es *suyo*, no de `GWorld`. El filtro
añadido ("que después acceda a `+0x160`/`+0x1D8`") no lo descartó porque, con
30.000 sitios, por pura estadística muchos acceden luego a esos offsets.

**Intento 2 — `0x9C1DAA0`.** Perfil limpio (1.366 lecturas, 2 escrituras, sin
`xor rsp`) y usos con `+0x1D8` y `+0x158`. Pero el ASM lo delata:
```asm
mov  rcx, cs:qword_149C1DAA0
test rcx, rcx
jz   short ...
mov  rcx, [rcx+0FB0h]        ; 0xFB0 no cabe en UWorld (sizeof 0x908)
```
Es `GEngine`, no `GWorld`.

**Por qué no se deja identificar estáticamente:** `GWorld` casi nunca aparece
como `mov reg,[GWorld]` seguido de `mov reg2,[reg+campo]`. Se carga y se pasa
como argumento a otras funciones, así que el patrón que delata a una global de
UE apenas se da. Un barrido de las globales más leídas (ya excluyendo el cookie)
no devuelve ninguna con accesos claros a campos de `UWorld`.

**Solución adoptada: localizar un ancla en runtime y confirmar el mundo en cada
frame** (`ScanForWorldAnchor` / `ReadValues`, en `reader.hpp`).

#### El error que hubo que corregir (parche 2026-09-03)

La primera versión buscaba un qword de las secciones de datos del módulo que
satisficiera esta cadena, y la reusaba como válida mientras siguiera
satisfaciéndola:

```
UWorld → OwningGameInstance(0x1D8) → LocalPlayers(0x38) → [0]
       → PlayerController(0x30) → PlayerCameraManager(0x360)
```

**Esa cadena no prueba nada sobre el mundo.** `UGameInstance` y `ULocalPlayer`
son objetos que **persisten entre mapas**: los crea el motor una vez y no se
destruyen al viajar. Por eso un `UWorld` ya destruido —cuya memoria sigue
mapeada— la sigue satisfaciendo entera. Consecuencia real observada en partida:
tras un cambio de mapa el cliente se quedaba anclado al mundo viejo, leía su
`GameState` (vacío o muerto), el ESP no dibujaba nada, y **como la validación
seguía pasando nunca se volvía a escanear**. Solo se arreglaba reinyectando.

#### Diseño actual

**1. Prueba de propiedad (lo que sí distingue un mundo de otro).**
Un `UWorld` es el actual si y solo si el `PlayerArray` de *su* `GameState`
contiene *tu* `APlayerState`:

```
ps  = PlayerController + 0x2B0            (AController::PlayerState)
gs  = candidato        + 0x160            (UWorld::GameState)
arr = gs + 0x2C0, num = gs + 0x2C8        (AGameStateBase::PlayerArray)
      -> ¿alguno de los num punteros es igual a ps?
```
No usa ni un offset nuevo: los tres están verificados por reflexión. Y en un
mapa nuevo tanto el `GameState` como los `APlayerState` son objetos nuevos, así
que un mundo viejo **falla la prueba de inmediato**.

**2. El ancla es solo una semilla.**
Se guarda la *dirección del hueco* de datos del módulo, no el mundo. Aunque ese
hueco quede desfasado, sigue sirviendo para llegar al `GameInstance`, que es
persistente. De ahí se saca el `PlayerController` actual y, con él, el mundo
actual por un camino independiente:

```
PlayerController → Outer(0x20) = ULevel → OwningWorld(0xC0) = UWorld actual
```
Si ese mundo pasa la prueba de propiedad, se usa **ese** y se re-ancla buscando
un hueco que lo contenga. La recuperación es inmediata, sin reescaneo completo.

> Este rescate es *seguro por construcción*: exige igualdad exacta de punteros
> contra un candidato que ya pasó la prueba del roster. Si `Outer` no fuera el
> `ULevel`, la lectura daría basura, la igualdad fallaría y simplemente no
> rescataría — nunca puede dar un falso positivo. En ese caso se recurre al
> reescaneo, que también arregla la situación, solo que más despacio.

**3. Red de seguridad escalonada.**
Si ni la semilla resuelve ni el mundo se confirma durante 120 frames seguidos
(y solo cuando el roster no está vacío, es decir, cuando *debería* poder
confirmarse), se fuerza un reescaneo completo. El umbral se **duplica** con cada
intento fallido hasta ~2 min, para que un escenario legítimamente no confirmable
no provoque un tirón cada dos segundos.

**4. El escaneo elige por niveles, no por "el primero que valga".**
Recorre todos los candidatos y se queda con el mejor:

| Nivel | Qué cumple |
|---|---|
| 3 | cadena completa **y** roster contiene tu `PlayerState` → es el mundo actual |
| 2 | cadena completa hasta `PlayerCameraManager` |
| 1 | solo llega al `GameInstance` (menú principal, sin controller) |

Corta en cuanto encuentra un nivel 3. Los niveles 1 y 2 mantienen el
comportamiento anterior en el menú, donde todavía no hay partida.

Ventajas frente al RVA fijo:
- No depende de acertar una heurística.
- **Sobrevive a los parches del juego sin tocar código** (comprobado: el parche
  del 2026-09-03 no lo rompió).
- Se confirma en cada frame contra un dato que solo el mundo actual tiene.

> El código heredado usaba `GWorld = 0x93CFB50`, de una build antigua.

---

## 3. Extracción masiva de clases

`GetPrivateStaticClassBody` (`sub_141378740`) se llama una vez por clase. Patrón
en los ~0x140 bytes anteriores al `call`:

```asm
mov  rax, cs:qword_149BE0D50        ; cache de la UClass (0 hasta el primer uso)
test rax, rax
jnz  ya_registrada
lea  r8,  qword_149BE0D50           ; 4C 8D 05 -> GLOBAL de la cache
lea  rdx, aAplayercameram+2         ; 48 8D 15 -> NOMBRE, UTF-16, apuntando a string+2
lea  rcx, aScriptEngine             ; 48 8D 0D -> PAQUETE, UTF-16 ("/Script/Engine")
mov  [rsp+...], 25A0h               ; C7 44 24 xx -> sizeof
call sub_141378740
```

Tres detalles que hacen fallar la extracción si se pasan por alto:
1. **El nombre se pasa como `string + 2`**: el binario guarda `"APlayerCameraManager"`
   y suma 2 bytes (1 carácter UTF-16) para saltar la `'A'`. Hay que retroceder 2.
2. **El paquete también es UTF-16**, no ASCII.
3. **La global va en `r8`**, o sea `4C 8D 05` (con REX.R), **no** `48 8D 05`.

Un cuarto detalle, aprendido al repetir la extracción sobre la build nueva:
4. **Hay tres enteros seguidos en la pila y el `sizeof` es el PRIMERO.** El orden
   real de los argumentos apilados es `InSize`, `InAlignment`, `ClassFlags`, o sea
   que en el flujo de bytes aparecen escritos al revés: primero `ClassFlags`
   (valores enormes tipo `0x10000004`), luego la alineación (`8` o `0x10`) y por
   último el `sizeof`. Quedarse con el primer entero que aparece da la
   **alineación**, no el tamaño. Ese fallo metió tres tamaños equivocados en la
   versión anterior de este documento (ver la nota bajo la tabla).

Resultado: **5.391 clases** con nombre, paquete, `sizeof` y RVA de su cache de
`StaticClass`. **156** pertenecen a `/Script/Bodycam`.
`GetPrivateStaticClassBody` está ahora en `0x1378880` (6.122 sitios de llamada).

> Las caches son **perezosas**: valen `0` hasta que el juego instancia esa clase.
> Un `0` significa "todavía no disponible", no "error".

### Validación cruzada `[XVAL]`
Todo offset de este documento cabe dentro del `sizeof` de su clase:

RVAs de `StaticClass` **de la build 2026-09-03** (los `sizeof` no cambiaron):

| Clase | `sizeof` | RVA StaticClass |
|---|---|---|
| `AActor` | `0x2A8` | `0x9be4da8` |
| `APawn` | `0x328` | `0x9c0aa78` |
| `ACharacter` | `0x650` | `0x9be9f88` |
| `AController` | `0x340` | `0x9bed838` |
| `APlayerController` | `0x858` | `0x9c0c100` |
| `APlayerState` | `0x360` | `0x9c0e608` |
| `APlayerCameraManager` | `0x25A0` | `0x9be1ed0` |
| `AGameStateBase` | `0x300` | `0x9bf5000` |
| `UWorld` | `0x908` | `0x9c223b0` |
| **`ULevel`** | **`0x320`** | `0x9bfe378` |
| `UGameInstance` | `0x1C0` | `0x9bf22a8` |
| `ULocalPlayer` | `0x2B0` | `0x9bfeb40` |
| `USceneComponent` | **`0x230`** | `0x9be2d98` |
| `USkinnedMeshComponent` | `0x890` | `0x9be3390` |
| `USkeletalMeshComponent` | `0xF40` | `0x9beccb8` |
| `UCapsuleComponent` | `0x510` | `0x9beb018` |

> **Corrección:** la versión anterior de este documento daba
> `USceneComponent = 0x2B8`, `UBodycamSurvivorComponent = 0x8D0` y
> `UShotgunAttributeSet = 0x130`. Los tres estaban mal por el fallo de extracción
> descrito arriba. Los valores reales, leídos del propio `call`, son `0x230`,
> `0x110` y `0x60`. Ninguna conclusión del documento cambia: todos los offsets
> siguen cabiendo. De hecho **dos encajan mejor ahora**:
> `ComponentToWorld` (`0x1D0`+`0x60` = `0x230`) resulta ser el último miembro de
> `USceneComponent`, y `DeathState@0x108` cabe justo en un
> `UBodycamSurvivorComponent` de `0x110`, lo que refuerza que ese es su dueño (§7).

### Clases de `/Script/Bodycam` relevantes

| Clase | `sizeof` | RVA StaticClass | Para qué |
|---|---|---|---|
| `ABodycamCharacter` | `0x680` | `0x9c65788` | pawn del jugador |
| `ABodycamWeaponCharacter` | `0x710` | `0x9c681b0` | pawn con arma (deriva del anterior `[DED]`) |
| `ABodycamPlayerState` | `0x438` | `0x9c67758` | equipo, nombre, kills |
| `ABodycamPlayerController` | `0x8B0` | `0x9c67740` | |
| `ABodycamGameState` | `0x420` | `0x9c66880` | roster |
| `ABodycamGameMode` | `0x6E0` | `0x9c66388` | |
| `ABodycamPlayerCameraManager` | `0x25A0` | `0x9c677e8` | cámara |
| `ABodycamSpectatorPawn` | `0x358` | `0x9c68168` | espectador (deriva de `APawn`, **no** de `ACharacter`) |
| `ABodycamControllablePerk` | `0x368` | `0x9c65728` | dron/perk controlable |
| `UCharacterAttributeSet` | `0xD8` | `0x9c68788` | **vida** |
| `UWeaponAttributeSet` | `0x240` | `0x9c68e20` | |
| `UShotgunAttributeSet` | `0x60` | `0x9c68e68` | |
| `UBodycamSurvivorComponent` | `0x110` | `0x9c682a0` | estado de muerte |
| `UBodycamAbilitySystemComponent` | `0x1260` | `0x9c65380` | GAS |
| `UBodycamTeamManagementComponent` | `0xB0` | `0x9c68138` | |
| `UBodycamEquipmentManagerComponent` | `0x140` | `0x9c65c10` | |
| `UBodycamInventoryComponent` | `0xF8` | `0x9c66e10` | |
| `UPawnExtensionComponent` | `0xE0` | `0x9c68bd8` | |
| `UBodycamRagdollComponent` | `0xC0` | `0x9c67c48` | |

---

## 4. Offsets del motor

### Tipos base
```
TArray  { void* Data @0x00 ; int32 Num @0x08 ; int32 Max @0x0C }   (16 B)
FString = TArray<wchar_t>  (UTF-16LE, incluye el terminador en Num)
FVector / FRotator = 3 × double (24 B)     ← LWC, NO floats
FQuat              = 4 × double (32 B)
FTransform (0x60): Rot @0x00 | Translation @0x20 | Scale3D @0x40   [ASM]
```
`sizeof(FTransform) = 0x60` verificado `[ASM]`: el `memcpy` de `USkinnedMeshComponent`
copia `32 * (3 * Num)` bytes = **96 B por elemento** (§6).

### UObject — **layout completo verificado** `[ASM]`
| Campo | Offset | Ev. |
|---|---|---|
| `UObject::ObjectFlags` | `0x08` | `[ASM]` |
| `UObject::InternalIndex` | `0x0C` | `[ASM]` (§2, indexado de `GUObjectArray`) |
| `UObject::Class` | `0x10` | `[ASM]` |
| `UObject::Name` (FName) | `0x18` | `[ASM]` |
| `UObject::Outer` | `0x20` | `[ASM]` |

Evidencia directa, en el registro de un `UObjectBase` recién construido
(`sub_1414A3DD0`), que pasa los cuatro campos como argumentos de golpe:
```asm
mov r9d, [rcx+8]      ; ObjectFlags
mov r8,  [rcx+18h]    ; NamePrivate  (FName)
mov rdx, [rcx+20h]    ; OuterPrivate
mov rcx, [rcx+10h]    ; ClassPrivate
call sub_1414A3B70
...                   ; y la otra rama de la misma función:
mov ebx, [rcx+0Ch]    ; InternalIndex
```
Y confirmación cruzada en `sub_1414ABE40`, que trata el `+0x10` como un objeto
por derecho propio y lo indexa en `GUObjectArray` por su `InternalIndex`:
```asm
mov rax, [rsi+20h]                   ; Outer
mov rbx, [rsi+10h]                   ; Class
mov eax, [rbx+0Ch]                   ; Class->InternalIndex
cmp eax, dword ptr cs:qword_149AA6184 ; NumElements
```

> Con esto `Class`, `Name` y `Outer` dejan de ser deducidos y salen de la lista
> de pendientes. Es lo que permite la prueba de propiedad del mundo (§2.1) y la
> detección de drones por nombre de clase (§7).

### ULevel
| Campo | Offset | Ev. |
|---|---|---|
| **`ULevel::OwningWorld`** | **`0xC0`** | `[REFL]` (`CPF_Transient`, `Object`) + `[ASM]` |
| `ULevel::LevelScriptActor` | `0xF0` | `[REFL]` |
| `ULevel::WorldSettings` | `0x2A8` | `[REFL]` |

`0xC0` cabe de sobra en `sizeof(ULevel) = 0x320` ✔ `[XVAL]`. El `[ASM]` es el
`GetLevel()->OwningWorld` que hace `AActor::GetWorld()`, visto inline en
`sub_1453E3FE0`:
```asm
mov rax, [rdi+20h]      ; Outer del actor -> ULevel
mov rcx, [rax+0C0h]     ; ULevel::OwningWorld
```
**El `Outer` de un actor es su `ULevel`.** Es el camino que usa el cliente para
recuperar el mundo actual tras un cambio de mapa (§2.1).

### Cadena mundo → cámara
| Ruta | Offset | Ev. |
|---|---|---|
| `UWorld::PersistentLevel` | `0x30` | `[REFL]` |
| `UWorld::AuthorityGameMode` | `0x158` | `[REFL]` |
| `UWorld::GameState` | `0x160` | `[REFL]` |
| `UWorld::Levels` | `0x178` | `[REFL]` |
| `UWorld::OwningGameInstance` | `0x1D8` | `[REFL]` |
| `UGameInstance::LocalPlayers` (TArray) | `0x38` | `[REFL]` |
| `ULocalPlayer::ViewportClient` | `0x78` | `[REFL]` |
| **`ULocalPlayer::AspectRatioAxisConstraint`** | **`0xB8`** | `[REFL]` uint8, `CPF_Config` |
| `ULocalPlayer::PendingLevelPlayerControllerClass` | `0xC0` | `[REFL]` |
| `ULocalPlayer::ControllerId` | `0xE0` | `[REFL]` |
| `ULocalPlayer::PlayerController` | `0x30` | `[REFL]` |
| `APlayerController::AcknowledgedPawn` | `0x350` | `[REFL]` |
| `APlayerController::PlayerCameraManager` | `0x360` | `[REFL]` |

> `AspectRatioAxisConstraint` es `UPROPERTY(config)`: el **offset** está verificado,
> pero el **valor** vive en los `.ini` del `.pak`. Hay que leerlo del proceso, no asumirlo.
> Enum: `0 = MaintainYFOV`, `1 = MaintainXFOV`, `2 = MajorAxisFOV`.

### APlayerCameraManager (`sizeof 0x25A0`)
| Campo | Offset | Ev. |
|---|---|---|
| `PCOwner` | `0x2A8` | `[REFL]` |
| `TransformComponent` | `0x2B0` | `[REFL]` |
| `DefaultFOV` | `0x2C0` | `[REFL]` |
| `DefaultOrthoWidth` | `0x2C8` | `[REFL]` |
| `DefaultAspectRatio` | `0x2D0` | `[REFL]` |
| `ViewTarget` | `0x340` | `[REFL]` |
| `PendingViewTarget` | `0xB90` | `[REFL]` |
| **`CameraCachePrivate`** | **`0x1410`** | `[REFL]` |
| **`LastFrameCameraCachePrivate`** | **`0x1C50`** | `[REFL]` |
| `ViewPitchMin` / `ViewPitchMax` | `0x256C` / `0x2570` | `[REFL]` |

`FCameraCacheEntry { float TimeStamp @0x00 ; FMinimalViewInfo POV @0x10 }`, tamaño `0x840`.
Se comprueba: `0x1C50 − 0x1410 = 0x840 = 0x10 + 0x830` ✔

### FMinimalViewInfo (`sizeof 0x830`, vía `SizeOfOuter` del bool param `[REFL]`)
| Campo | Rel. | Abs. (cache actual) | Abs. (frame anterior) |
|---|---|---|---|
| `Location` | `0x00` | **`0x1420`** | `0x1C60` |
| `Rotation` | `0x18` | `0x1438` | `0x1C78` |
| `FOV` | `0x30` | **`0x1450`** | `0x1C90` |
| `DesiredFOV` | `0x34` | `0x1454` | `0x1C94` |
| `FirstPersonFOV` | `0x38` | `0x1458` | |
| `FirstPersonScale` | `0x3C` | `0x145C` | |
| `OrthoWidth` | `0x40` | `0x1460` | |
| `PerspectiveNearClipPlane` | `0x58` | `0x1478` | |
| `AspectRatio` | `0x5C` | **`0x147C`** | `0x1CBC` |
| bitfield | `0x68` | **`0x1488`** | `0x1CC8` |
| `ProjectionMode` | `0x6C` | `0x148C` | |
| `PostProcessSettings` | `0x80` | `0x14A0` | |
| `OffCenterProjectionOffset` | `0x780` | | |
| `CropFraction` | `0x804` | | |

Bits del dword `@0x68`, sacados de sus `SetBitFunc` `[ASM]`:
```asm
sub_143428CF0:  or dword ptr [rcx+68h], 1     ; bConstrainAspectRatio    = 0x01
```
`bUseFieldOfViewForLOD` y `bUseFirstPersonParameters` ocupan los bits contiguos `[DED]`.

### Actor y componentes
| Campo | Offset | Ev. |
|---|---|---|
| `AActor::Owner` | `0x158` | `[REFL]` |
| `AActor::Instigator` | `0x1A0` | `[REFL]` |
| **`AActor::RootComponent`** | **`0x1B8`** | `[REFL]` + `[ASM]` |
| `USceneComponent::RelativeLocation` | `0x128` | `[REFL]` |
| `USceneComponent::RelativeRotation` | `0x140` | `[REFL]` |
| `USceneComponent::ComponentVelocity` | `0x170` | `[REFL]` |
| **`USceneComponent::ComponentToWorld`** | **`0x1D0`** | `[ASM]` |
| → `.Translation` | `0x1F0` | `[ASM]` |
| → `.Scale3D` | `0x210` | `[ASM]` |

`ComponentToWorld` **no es UPROPERTY** (no sale por reflexión). Evidencia directa —
esto es `AActor::GetActorLocation()` inline:
```asm
mov    rax, [rcx+1B8h]              ; RootComponent
test   rax, rax
jz     short (devolver vector cero)
movups xmm0, xmmword ptr [rax+1F0h] ; ComponentToWorld.Translation.X,Y
movups xmm0, xmmword ptr [rax+200h] ; ...Z
```
Y la copia del `FTransform` entero, que fija la base en `0x1D0`:
```asm
movups xmm0, xmmword ptr [rax+1D0h] ; Rot.X,Y
movups xmm1, xmmword ptr [rax+1E0h] ; Rot.Z,W
movups xmm0, xmmword ptr [rax+1F0h] ; Translation.X,Y
movups xmm1, xmmword ptr [rax+200h] ; Translation.Z
```
`0x1D0 + 0x60 = 0x230 ≤ 0x2B8` (`sizeof USceneComponent`) ✔ `[XVAL]`

### Pawn / Controller / Character
| Campo | Offset | Ev. |
|---|---|---|
| `APawn::BaseEyeHeight` | `0x2B4` | `[REFL]` |
| `APawn::RemoteViewPitch` (uint8) | `0x2BA` | `[REFL]` |
| `APawn::PlayerState` | `0x2C8` | `[REFL]` |
| `APawn::Controller` | `0x2D8` | `[REFL]` |
| `AController::PlayerState` | `0x2B0` | `[REFL]` |
| `AController::Pawn` | `0x2E8` | `[REFL]` |
| `AController::Character` | `0x2F8` | `[REFL]` |
| `AController::ControlRotation` | `0x320` | `[REFL]` |
| `ACharacter::Mesh` | `0x328` | `[REFL]` |
| `ACharacter::CharacterMovement` | `0x330` | `[REFL]` |
| `ACharacter::CapsuleComponent` | `0x338` | `[REFL]` |
| `UCapsuleComponent::CapsuleHalfHeight` | `0x508` | `[REFL]` |
| `UCapsuleComponent::CapsuleRadius` | `0x50C` | `[REFL]` |

> `CapsuleHalfHeight` **cambia** al agacharse/tumbarse: hay que releerlo cada frame
> si se quiere una caja que siga la postura. Los dos campos son adyacentes → una
> sola lectura de 8 bytes.

### Roster
| Campo | Offset | Ev. |
|---|---|---|
| `AGameStateBase::GameModeClass` | `0x2A8` | `[REFL]` |
| `AGameStateBase::AuthorityGameMode` | `0x2B0` | `[REFL]` |
| `AGameStateBase::SpectatorClass` | `0x2B8` | `[REFL]` |
| **`AGameStateBase::PlayerArray`** | **`0x2C0`** | `[REFL]` `TArray<APlayerState*>` |
| `AGameStateBase::ReplicatedWorldTimeSeconds` | `0x2D4` | `[REFL]` |
| `APlayerState::Score` | `0x2A8` | `[REFL]` |
| `APlayerState::PawnPrivate` | `0x320` | `[REFL]` |
| `APlayerState::PlayerNamePrivate` (FString) | `0x340` | `[REFL]` |

Recorrido correcto del roster:
```
gs   = *(void**)(world + 0x160)
data = *(void**)(gs + 0x2C0)
num  =  *(int*)(gs + 0x2C8)          // acotar a [0, 128]
ps_i = *(void**)(data + i*8)         // stride 8, NO 0x318
pawn = *(void**)(ps_i + 0x320)       // PawnPrivate
```

---

## 5. World-to-screen correcto

`FMinimalViewInfo::FOV` no dice qué eje conserva. Lo decide
`ULocalPlayer::AspectRatioAxisConstraint`, replicando
`CalculateProjectionMatrixGivenViewRectangle`:

```
si bConstrainAspectRatio        -> xMult = 1,    yMult = AspectRatio
si no, si (W>H && axis==2) || axis==1 -> xMult = 1,    yMult = W/H
si no                           -> xMult = H/W,  yMult = 1
```

```
d       = WorldPos - POV.Location
(fwd, right, up) = ejes de la matriz de rotación de POV.Rotation
depth   = dot(d, fwd)
si depth <= 1.0  ->  NO VISIBLE   (¡rechazar, no recortar!)
tanHalf = tan(FOV * PI / 360)
ScreenX = W/2 + dot(d,right)/depth * (xMult/tanHalf) * (W/2)
ScreenY = H/2 - dot(d,up)   /depth * (yMult/tanHalf) * (H/2)
```

**Síntoma de tenerlo mal:** las cajas se van hacia afuera del centro, y cada vez
más cuanto más cerca del borde. Ese es exactamente el bug del `WorldToScreen.hpp`
heredado, que usaba `widthscreen/2` también en el eje Y.

**W y H deben ser el tamaño del cliente del juego**, no `GetSystemMetrics(SM_CXSCREEN)`.
Con el juego en ventana o a resolución distinta de la del escritorio, la del
escritorio da un resultado incorrecto.

### Corrección de escala (`FOV Scale`)
El `FOV` que publica `FMinimalViewInfo` no tiene por qué ser el que el juego usa
para construir la matriz de proyección: puede haber un factor propio del título.
El síntoma es inconfundible: **cuanto más cerca del borde de la pantalla está el
enemigo, más se desplaza su caja**. Se corrige dividiendo el `tanHalf`:

```
tanHalf = tan(FOV * PI / 360) / FovScale
```

`FovScale = 1.0` usa el FOV tal cual. Valores > 1 abren la proyección y acercan
las cajas al centro.

### Valores calibrados en partida (Bodycam)

| Ajuste | Valor | Nota |
|---|---|---|
| `FOV Scale` | **1.150** | calibrado en partida |
| `FOV Axis` | **Force X-FOV** | `g_AxisOverride = AspectAxis_MaintainXFOV` |

Ambos son ya los valores **por defecto** del cliente, y siguen siendo ajustables
desde el menú por si cambian con la resolución o el FOV del juego.

> **Discrepancia documentada:** en partida se observó `FOV 120.0`,
> `AspectRatio 1.778`, `bConstrainAspectRatio 0` y `AspectRatioAxisConstraint`
> (`ULocalPlayer+0xB8`) leído = **`0` (MaintainYFOV)**. Sin embargo la
> proyección real se comporta como **MaintainXFOV**. El offset está verificado
> por reflexión, así que o el valor de esa propiedad no es el que acaba usando
> el render, o el juego aplica otro criterio. Como el comportamiento observado
> es inequívoco, se fuerza X-FOV y se deja anotado.
>
> Consecuencia en el código: cuando el usuario fuerza un eje a mano, ese eje
> tiene **prioridad sobre `bConstrainAspectRatio`**. Si no, la rama de aspecto
> restringido se ejecutaría antes y el forzado quedaría ignorado en silencio.

---

## 6. Huesos

### USkinnedMeshComponent (`sizeof 0x890`)
| Campo | Offset | Ev. |
|---|---|---|
| `SkeletalMesh` (deprecada) | `0x520` | `[REFL]` |
| **`SkinnedAsset`** | **`0x528`** | `[REFL]` |
| **`LeaderPoseComponent`** | **`0x530`** | `[REFL]` + `[ASM]` |
| `MeshDeformerInstances` | `0x568` | `[REFL]` |
| **`ComponentSpaceTransformsArray[i].Data`** | **`0x598 + 0x10*i`** | `[ASM]` |
| **`…[i].Num`** | **`0x5A0 + 0x10*i`** | `[ASM]` |
| `CurrentEditableComponentTransforms` | `0x5DC` | `[ASM]` |
| **`CurrentReadComponentTransforms`** | **`0x5E0`** | `[ASM]` |
| `LeaderBoneMap.Data` / `.Num` | `0x608` / `0x610` | `[ASM]` |
| `PhysicsAssetOverride` | `0x708` | `[REFL]` |
| `VisibilityBasedAnimTickOption` | `0x764` | `[REFL]` |
| `USkeletalMeshComponent::AnimScriptInstance` | `0x8A0` | `[REFL]` |
| `USkeletalMeshComponent::AnimClass` | `0x898` | `[REFL]` |

Los arrays **no son UPROPERTY**. Evidencia dura, decompilación de `sub_143E70FB0`:
```c
v4  = resolve(*a1 + 1328);              // 0x530  LeaderPoseComponent
v13 = v4 + 16LL * *(int *)(v4 + 1504);  // 0x5E0  CurrentReadComponentTransforms, ×16
v14 = *(int *)(v13 + 1440);             // 0x5A0  .Num
v15 = *(const void **)(v13 + 1432);     // 0x598  .Data
memcpy(dst, v15, 32 * (3 * v14));       // 96 B por FTransform  ⇒ sizeof = 0x60
```
y en ASM, la otra rama del mismo sitio:
```asm
cmp    byte ptr [rdi+768h], 0
jge    short ...
movsxd rax, dword ptr [rdi+5DCh]   ; CurrentEditableComponentTransforms
add    rdi, 598h                   ; base del array de TArrays
```

### Leer un hueso
```
mesh  = pawn + 0x328                          (ACharacter::Mesh)
bm    = LeaderPoseComponent(mesh+0x530) ?: mesh
idx   = clamp(*(int32*)(bm + 0x5E0), 0, 1)
arr   = bm + 0x598 + idx*0x10
data  = *(void**)arr ;  num = *(int32*)(arr+8)      // acotar num a (0, 1024]
boneT = data + i*0x60                                // FTransform del hueso i
local = *(FVector*)(boneT + 0x20)                    // Translation, espacio de componente
world = ComponentToWorld(mesh + 0x1D0) aplicado a boneT
```

> **La pose y la jerarquía son caminos independientes.** Se pueden leer los 90
> huesos perfectamente y aun así no dibujar nada porque falló el `RefSkeleton`.
> Hay que loguear los dos pasos por separado o el diagnóstico es imposible.

### FReferenceSkeleton (jerarquía)
`GetRefSkeleton()` es **virtual, slot `0x330`** del `USkinnedAsset` `[ASM]`:
```asm
mov  rcx, [rsi+10h]        ; USkinnedAsset*
mov  r8,  [rcx]            ; vtable
call qword ptr [r8+330h]   ; -> FReferenceSkeleton*
mov  ecx, [rax+28h]        ; FinalRefBoneInfo.Num
cmp  [rdx+610h], ecx       ; contra LeaderBoneMap.Num
```

Layout `[ASM]` para `FinalRefBoneInfo`, resto `[DED]` por simetría con UE5:
```
FReferenceSkeleton:
  RawRefBoneInfo    TArray<FMeshBoneInfo>  @0x00
  RawRefBonePose    TArray<FTransform>     @0x10
  FinalRefBoneInfo  TArray<FMeshBoneInfo>  @0x20   ← Data@0x20, Num@0x28  [ASM]
  FinalRefBonePose  TArray<FTransform>     @0x30
  RawNameToIndexMap TMap                   @0x40

FMeshBoneInfo (stride 0x0C):  FName Name @0x00 ; int32 ParentIndex @0x08
```

**Cómo resolverlo sin llamar al juego.** Llamar al virtual desde el hilo del
overlay es innecesario y arriesgado. En su lugar se localiza el offset del
`FReferenceSkeleton` dentro del asset **escaneando una sola vez** y validando
con invariantes que ningún dato basura cumple:
- `Num` en `(0, 1024]`
- `ParentIndex[0] == -1` (la raíz no tiene padre)
- `ParentIndex[i] < i` para todo `i` (los padres siempre van antes)
- `ParentIndex[i] >= -1`

El offset se cachea globalmente y los padres por asset. **No cachear el fallo**:
si un frame no resuelve, hay que reintentar.

> `parents.size()` puede ser **mayor** que el número de huesos de la pose (LODs,
> huesos virtuales). Usar `min(...)`, no exigir igualdad, o el esqueleto no se
> dibuja sin motivo aparente.

### Identificar la cabeza sin nombres de hueso
No hace falta `GNames`. En la **pose de referencia** (T-pose, `RawRefBonePose`)
la cabeza es siempre el hueso de mayor Z acumulada por la cadena de padres.
Se calcula una vez por asset y se cachea. Es estable, al contrario que mirar la
pose actual (que cambia al agacharse o tumbarse).

Alternativa que evita el problema entero: **la caja se calcula como el bounding
box de todos los huesos proyectados**. Sale ajustada a la postura real y no
necesita saber qué hueso es cuál.

### Guardia contra esqueletos disparados

Leer la pose de un pawn que **no es un `ACharacter`** (un dron, un espectador)
significa leer `+0x328` como si fuera `Mesh`, y ahí hay otra cosa. Si por
casualidad ese qword es un puntero válido, todo lo demás se lee sin fallar y el
resultado son líneas que salen disparadas por el mapa. Pasó con el parche del
2026-09-03, porque al romperse `GNames` los drones dejaron de detectarse.

Antes de transformar los huesos, se comprueban dos cosas sobre el
`ComponentToWorld` de la malla, y si falla cualquiera se descarta la pose entera
(contador `badMesh` en *Debug → Bone counters*):

1. **La transformada tiene sentido**: cuaternión con norma² en `[0.5, 2]`, escala
   en `[1e-4, 100]` en valor absoluto, traslación finita y por debajo de `1e7` cm.
2. **La malla pertenece al actor**: su traslación está a menos de **5 m** de la
   del `RootComponent` del pawn. En un personaje real la distancia es de ~1 m
   (la malla cuelga del centro de la cápsula), así que 5 m no descarta nada
   legítimo. Si no se puede leer el `RootComponent`, la comprobación se salta:
   nunca esconde un esqueleto válido por no poder verificarlo.

---

## 7. Datos del juego

### Vida — GameplayAbilities
```
ABodycamCharacter + 0x668  ->  UCharacterAttributeSet*     [REFL] "CharacterSet"
```
`UCharacterAttributeSet` (`sizeof 0xD8`) `[REFL]`:

| Atributo | Offset del `FGameplayAttributeData` | `CurrentValue` |
|---|---|---|
| `Health` | `0x88` | **`0x94`** |
| `MaxHealth` | `0x98` | **`0xA4`** |
| `GadgetCooldown` | `0xA8` | `0xB4` |
| `Healing` | `0xB8` | `0xC4` |
| `Damage` | `0xC8` | `0xD4` |

`FGameplayAttributeData` (16 B) `[REFL]`:
```
vtable @0x00 | float BaseValue @0x08 | float CurrentValue @0x0C
```
El espaciado de `0x10` entre los cinco atributos y el cierre exacto en
`0xC8 + 0x10 = 0xD8 = sizeof` confirman el layout ✔ `[XVAL]`

> **Usa `CurrentValue` (`+0x0C`), no `BaseValue`.** `BaseValue` es el valor sin
> modificadores de GAS; `CurrentValue` es la vida que ve el jugador.

### ABodycamCharacter (props propias, `0x650`…`0x680`) `[REFL]`
| Campo | Offset |
|---|---|
| `AbilitySystemComponent` | `0x658` |
| `PawnExtComponent` | `0x660` |
| **`CharacterSet`** | **`0x668`** |
| `CachedController` | `0x670` |

> El código heredado llamaba `SurvivorStatus = 0x668` a este mismo offset. El
> **offset sigue siendo válido**; lo que cambió es el contenido: hoy apunta a un
> `UCharacterAttributeSet` con `float`s, no a una estructura con un `double @0xB0`.

### ABodycamControllablePerk (props propias, `0x328`…`0x368`) `[REFL]`
`AbilitySystemComponent@0x338`, `PawnExtComponent@0x340`, `CharacterSet@0x348`,
`WeaponSet@0x350`, `PerkSet@0x358`, `CharacterOwner@0x360`.
Se distingue de `ABodycamCharacter` por el `sizeof` de la clase base (deriva de
`APawn 0x328`, no de `ACharacter 0x650`).

### UWeaponAttributeSet (`sizeof 0x240`) `[REFL]`
33 atributos GAS, todos `FGameplayAttributeData` de 16 B (usar `+0x0C` para el
`CurrentValue`). El último cae en `0x230 + 0x10 = 0x240` = `sizeof` exacto ✔ `[XVAL]`

| Offset | Atributo | | Offset | Atributo |
|---|---|---|---|---|
| `0x30` | `SpreadX` | | `0x130` | `CameraRecoilStepHorizontal` |
| `0x40` | `SpreadY` | | `0x140` | `KickUp` |
| `0x50` | `CameraRecoilStep` | | `0x150` | `KickHip` |
| `0x60` | `Damage` | | `0x160` | `MagnifiedKickHip` |
| `0x70` | `RagdollForce` | | `0x170` | `MagnifiedKickUp` |
| `0x80` | `ADSSpeed` | | `0x180` | `MagnifiedMaxKickHip` |
| `0x90` | `Sway` | | `0x190` | `MagnifiedMaxKickUp` |
| `0xA0` | `SwayPivotOffset` | | `0x1A0` | `FireRate` |
| `0xB0` | `Instability` | | `0x1B0` | `ReloadTime` |
| `0xC0` | `SwitchSpeed` | | `0x1C0` | `MagAmount` |
| `0xD0` | `Speed` | | `0x1D0` | `AmmoCapacity` |
| `0xE0` | `VerticalRecoil` | | `0x1E0` | `CurrentAmmo` |
| `0xF0` | `HorizontalRecoil` | | `0x1F0` | `PelletCount` |
| `0x100` | `RecoilCompensation` | | `0x200` | `SpreadAimX` |
| `0x110` | `RecoilRecoverySpeed` | | `0x210` | `SpreadAimY` |
| `0x120` | `CameraRecoilStepVertical` | | `0x220` | `SpreadHipX` |
| | | | `0x230` | `SpreadHipY` |

> No está resuelto cómo se llega a este set desde el pawn. Y al ser atributos
> GAS replicados, escribirlos en el cliente probablemente no cambie lo que
> calcula el servidor: **no verificado**, no asumir que sirven para no-recoil.

### ABodycamPlayerState (props propias, `0x360`…`0x438`) `[REFL]`
| Campo | Offset | Nota |
|---|---|---|
| `OnTeamIdChanged` | `0x368` | delegado |
| `OnScoreChanged` | `0x378` | delegado |
| **`TeamId`** | **`0x388`** | int32, `CPF_Net` ✔ |
| `Kill` | `0x38C` | int32 |
| `Death` | `0x390` | int32 |
| `bIsLateJoiningPlayer` | bitfield | |
| `bIsSpectatorState` | bitfield | |
| `Friends` | `0x398` | TArray |
| `GadgetExpirationTime` | `0x3F8` | |
| `LastRespawnTime` | `0x400` | |
| `SpawnCount` | `0x408` | |
| `PartyId` | `0x40C` | |
| `CachedPlatformId` | `0x420` | |

**El equipo se lee del PlayerState, nunca del pawn.** `TeamId` lleva `CPF_Net`
(flags `0x20080100020035`), así que está replicado y es fiable en cliente.

### Input de rotación de la vista — `APlayerController` `[ASM]`

`RotationInput` **no es UPROPERTY**. Se localizó desensamblando las tres
funciones `AddXInput`, que hacen literalmente `RotationInput.X += val * Escala`:

| Campo | Offset | Ev. |
|---|---|---|
| **`RotationInput.Pitch`** | **`0x528`** | `[ASM]` |
| **`RotationInput.Yaw`** | **`0x530`** | `[ASM]` |
| **`RotationInput.Roll`** | **`0x538`** | `[ASM]` |
| `InputYawScale` | `0x540` | `[REFL]` |
| `InputPitchScale` | `0x544` | `[REFL]` |
| `InputRollScale` | `0x548` | `[REFL]` |

Que las tres escalas reflexionadas caigan justo detrás del `FRotator` de 24 B
(`0x528`+`0x18` = `0x540`) confirma el layout ✔ `[XVAL]`

**`ControlRotation` es un resultado, no una entrada.** Cada tick el juego ejecuta
`UpdateRotation()` y lo recalcula a partir de `RotationInput`. Por eso escribir
`ControlRotation` directamente pelea con el motor: se escribe un valor que el
siguiente tick descarta. Lo correcto es alimentar `RotationInput`.

### Componente de estado vital `[REFL]`
Grupo de props con `InitialHealth@0xF0`, `AbilitySystemComponent@0xF8`,
`CharacterAttributeSet@0x100`, **`DeathState@0x108`** (enum), y los delegados
`OnHealthChanged@0xA0`, `OnMaxHealthChanged@0xB0`, `OnRevive@0xC0`,
`OnDeathStarted@0xD0`, `OnDeathFinished@0xE0`.
Es casi seguro `UBodycamSurvivorComponent` `[DED]` — **no confirmé el dueño**, y
no hay una prop reflexionada que lleve del pawn a este componente.
Por eso, para saber si alguien está vivo, **usar `Health > 0`**, que sí es un
camino verificado de punta a punta.

---

## 8. Estado del código heredado (antes de esta sesión)

`ImGuiExternal/` es una **DLL inyectada**: `DllMain` lanza un hilo que crea una
ventana overlay propia con D3D9 + ImGui y lee la memoria del juego con punteros
directos (no `ReadProcessMemory`).

### Compilación

| | Debug\|x64 | Release\|x64 |
|---|---|---|
| Tipo | DynamicLibrary | DynamicLibrary |
| Salida | `x64\Debug\NOVA.dll` | `x64\Release\NOVA.dll` |
| Runtime | `/MDd` (dinámico, **necesita VS instalado**) | `/MT` (**estático, sin dependencias**) |
| Optimización | ninguna | `/O2 /GL` + `/OPT:REF /OPT:ICF` |
| Tamaño | ~2,2 MB | ~618 KB |

`Release|x64` estaba mal configurado de origen: era **`Application`**, así que no
producía una DLL válida y nunca se había usado. Se corrigió y se alineó con Debug
(tipo DLL, `_CRT_SECURE_NO_WARNINGS`, rutas de include, `SubSystem=Windows`,
`/ignore:4099`).

**Runtime estático en Release (`/MT`) a propósito**: así el DLL solo importa
`kernel32`, `user32`, `imm32`, `d3d9` y `dwmapi`, y carga en cualquier Windows sin
el redistribuible de Visual C++. Es seguro porque no se comparte ningún objeto del
CRT con el proceso anfitrión (no se reserva memoria a un lado para liberarla al
otro, ni se pasan `FILE*`).

No hay ningún `#if _DEBUG` / `NDEBUG` ni `assert()` de runtime en el código, así
que **ambas configuraciones compilan exactamente lo mismo** y se comportan igual.

### Offsets que estaban bien
`game_instance 0x1D8`, `game_state 0x160`, `local_player 0x38`,
`player_controller 0x30`, `camera_manager 0x360`, `acknowledged_pawn 0x350`,
`player_state 0x2C8`, `root_component 0x1B8`, `relative_location 0x128`,
`control_rotation 0x320`, `skeletal_mesh_component 0x328`,
`component_to_world 0x1D0`, `active_bone_array 0x598`, `bone_buffer_index 0x5E0`,
`leader_pose_component 0x530`, `bone_stride 0x60`, `pov_info 0x1420`.

### Offsets rotos
| Nombre | Valor viejo | Real | Consecuencia |
|---|---|---|---|
| `Uworld` (RVA) | `0x93CFB50` | **se busca en runtime** (§2.1) | nada funciona |
| `player_array` | `0x338` | **`0x2C0`** | roster vacío o basura (¡no cabe en la clase!) |
| `player_array_stride` | `0x318` | **`8`** | se lee fuera del array |
| `player_array_data` | `0x8` | **`0`** | desalineado |
| `health` | `0xB0` (double) | **`0x94`** (float) | vida siempre inválida |
| `team_index_pawn` | `0xF49` en el pawn | **`0x388` en el PlayerState** | equipo siempre 0 |
| `cached_bone_array` | `0x960` | no existe | fallback muerto |

### Bugs de lógica encontrados
1. **`w2s` no rechaza lo que está detrás de la cámara** (`if (z<1) z=1`) → los
   enemigos de detrás se dibujan delante y en espejo.
2. **`w2s` usa `widthscreen/2` en el eje Y** → trampa B, cajas desplazadas.
3. **`w2s` descarta posiciones válidas**: `if (x==0 || y==0 || z==0) return {0,0}`
   rechaza cualquier punto con una coordenada exactamente 0.
4. **Resolución equivocada**: `GetSystemMetrics(SM_CXSCREEN)` en vez del tamaño
   del cliente del juego.
5. **Desreferencias crudas sin validar**: `*(int*)(pawn + team_index_pawn)` y
   `*(int*)(acknowledged_pawn + …)` pueden tumbar el proceso del juego.
6. **`VirtualQuery` en cada lectura**: una syscall por hueso y por jugador. Con
   20 huesos × 10 jugadores son 200 syscalls por frame solo para el esqueleto.
7. **`clearVariable(x)` hace `Release()` y después `delete x`** → doble
   liberación de un objeto COM. Crash al cerrar.
8. **Índices de hueso hardcodeados** (`head = 48`, `neck = 5`…) sin ninguna
   validación de que correspondan al esqueleto de este juego.
9. `DrawBones` captura con `catch(...)` accesos inválidos de memoria, que en
   x64 son excepciones SEH, no C++: **no las captura** salvo con `/EHa`.

---

## 9. Reglas de seguridad (proyecto in-process)

Esto es una DLL dentro del juego: un fallo aquí **tumba el juego**, no al cheat.

- **Nunca desreferenciar un puntero del juego directamente.** Todo acceso pasa
  por el lector con validación + SEH.
- Validar todo puntero antes de usarlo: no nulo, canónico
  (`0x10000 ≤ p < 0x7FFFFFFFFFFF`), alineado.
- **`__try/__except` no convive con objetos con destructor** en la misma función
  (error C2712). Aislar la lectura en su propia función sin objetos C++.
- Acotar todo recorrido: `PlayerArray.Num ∈ [0,128]`, `bones ∈ (0,1024]`,
  `LocalPlayers ∈ [1,8]`, `FString.Num ∈ (0,256]`, cadena de padres ≤ 256.
- **Nunca cachear punteros de objetos del juego entre frames.** Se resuelve la
  cadena entera cada frame. Cachear solo datos inmutables (jerarquía por asset).
- Posiciones y rotaciones en `double` (LWC); pasar a `float` solo al dibujar.
- Un offset equivocado debe producir datos raros, **jamás un crash**.
- Nada de escrituras salvo donde ya las había (aimbot), y con el mismo cuidado.

### 9.1 Funciones nativas verificadas y llamables

Tres funciones de `APlayerController`, contiguas y de `0x8C` bytes cada una:

| Función | RVA (2026-09-03) | RVA anterior | Escribe | Escala |
|---|---|---|---|---|
| `AddPitchInput(float)` | **`0x3CB83C0`** | `0x3CB80F0` | `RotationInput.Pitch @0x528` | `InputPitchScale @0x544` |
| `AddRollInput(float)` | **`0x3CB8450`** | `0x3CB8180` | `RotationInput.Roll @0x538` | `InputRollScale @0x548` |
| `AddYawInput(float)` | **`0x3CB85D0`** | `0x3CB8300` | `RotationInput.Yaw @0x530` | `InputYawScale @0x540` |

> El parche del 2026-09-03 las movió `0x2D0` bytes, **manteniendo el espaciado
> relativo entre ellas** (`+0x90` y `+0x180`) y **sin tocar los offsets del
> `FRotator`** (`0x528`/`0x530`/`0x538` siguen igual). Los RVAs de esta tabla son
> solo una pista: el cliente los verifica y, si no cuadran, **los busca por firma**
> (§2.3). Se comprobó sobre el binario nuevo que la búsqueda devuelve exactamente
> `0x3CB83C0` y `0x3CB85D0`.

Cuerpo (decompilado, `AddYawInput`):
```c
if (IsLookInputIgnored())            // virtual, slot 0x848
    delta = 0.0;
else
    delta = (flagEscalaDeprecada ? InputYawScale : 1.0f) * val;
RotationInput.Yaw += delta;          // double @0x530
```

**Firma para localizar y verificar en runtime.** Prólogo en `+0x00` y cola en
`+0x73`; el offset del `FRotator` va incrustado dos veces en la cola, así que
**la firma distingue el eje** y solo hay 3 coincidencias en todo el binario, una
por eje. Se usa para las dos cosas: comprobar el RVA conocido y, si falla,
buscarlas de cero recorriendo `.text`:
```
+0x00  40 53                    push rbx
       48 83 EC 30              sub  rsp, 30h
       48 8B 01                 mov  rax, [rcx]
       48 8B D9                 mov  rbx, rcx
+0x73  0F 5A C0                 cvtps2pd xmm0, xmm0
       F2 0F 58 83 <off32>      addsd  xmm0, [rbx+off]
       F2 0F 11 83 <off32>      movsd  [rbx+off], xmm0
       48 83 C4 30 5B C3        add rsp,30h ; pop rbx ; ret
```
El último byte comprobado es `0x73 + 24 = 0x8B`, dentro de los `0x8C` de la
función: la verificación no se sale.

**Convención**: `void __fastcall f(APlayerController* rcx, float xmm1)`. Lo
confirma el propio prólogo (`mov rbx, rcx` y `movaps xmm6, xmm1`).

**Por qué estas dos sí se pueden llamar desde el hilo del overlay.** Consultan
un flag, leen un float de configuración y suman a un double. No reasignan
punteros, no tocan contenedores ni el scene graph. Lo peor que puede pasar es
una carrera con el tick del juego que pierda un incremento — un microtirón, no
corrupción. Esto **no generaliza**: `AController::SetControlRotation`, por
ejemplo, puede acabar en `RootComponent->SetWorldRotation()` y tocar el scene
graph; esa hay que ejecutarla desde el game thread.

**Escala efectiva**: `InputYawScale`/`InputPitchScale` están deprecadas en UE5 y
un flag decide si se aplican o se usa `1.0`. No se puede dar por supuesto, así
que el cliente **la mide en runtime**: manda un valor de sondeo minúsculo
(0.05), compara lo que entró en `RotationInput` y de ahí saca la escala real,
incluido el signo (`InputPitchScale` suele ser negativo).

### Sobre llamar funciones del juego
Es técnicamente posible desde una DLL (vía `UObject::ProcessEvent` o llamando
directo a una función encontrada por firma), pero tiene un riesgo real y hay que
tratarlo aparte del ESP:

- **El hilo importa.** El overlay corre en su propio hilo. Casi todo el código de
  gameplay de UE asume el *game thread*. Llamar desde otro hilo corrompe estado y
  produce crashes no deterministas, a veces minutos después.
- La forma correcta es **encolar** la llamada y ejecutarla desde un hook en una
  función que ya corra en el game thread, no invocarla directamente.
- No puede causar un BSOD: todo esto es user-mode. El riesgo es cerrar el juego.
- Requiere tres piezas. Estado real:

| Pieza | Estado |
|---|---|
| `GUObjectArray` | ✅ **verificado** (§2) — ya se pueden enumerar todos los objetos |
| `GNames` (`FNamePool`) | ✅ **verificado** (§2.2) — `0x99C29C0`, localizado por firma, ya se usa en el cliente para nombres de hueso y de clase |
| `ProcessEvent` | ❌ **no localizado**. Es lo único que falta. Ancla: la cadena wide "Script call stack" (ojo: su dirección cambió con el parche, hay que volver a buscarla) |

> Al buscar `GNames` hay una trampa: el patrón "`>>16` + índice `*8` + `lea` con
> `*2`" lo cumple también `GUObjectArray` (909 aciertos frente a 1). Se
> distinguen porque en `GUObjectArray` el índice y la base del `lea` son el
> **mismo registro** (es un `*3`), y en `FNamePool` son distintos. Ver §2.2.

### 9.2 Soft aim / silent aim — por qué NO se implementó el "puro"

Objetivo pedido: que la bala vaya a la cabeza **sin mover la vista**. Eso exige
interceptar el cálculo de la dirección del disparo dentro del juego. Se
investigó y **no hay base para hacerlo con garantías**:

1. **La función de disparo no está identificada.** Se buscaron 30 nombres
   nativos típicos (`Fire`, `WeaponTrace`, `TraceBullet`, `GetShotDirection`,
   `PerformTrace`, `FireProjectile`…) y en todo el binario solo aparecen
   `StartFire` (que es la de `APlayerController`, no la del arma) y
   `LineTraceSingle`. Es un indicio fuerte de que **la lógica de disparo vive en
   Blueprint**, no en C++ nativo. Si es así no hay función nativa que hookear:
   habría que interceptar `ProcessEvent` y operar sobre bytecode de Blueprint.
2. **La búsqueda por atributos tampoco la encuentra.** Rastrear quién lee
   `SpreadAimX/Y`, `SpreadHipX/Y` y `PelletCount` del `UWeaponAttributeSet`
   devuelve 754 funciones, y las mejores candidatas son de 9 KB con 187
   llamadas: no son el trace de una bala, los offsets coinciden por casualidad.
3. **El proyecto no tiene motor de hooks utilizable.** `hookclass::Hook`
   escribe un `jmp` absoluto de 14 bytes **sobre** la función y no conserva las
   instrucciones desplazadas, así que no permite llamar a la original. Para esto
   haría falta MinHook o un trampolín con desensamblador de longitudes.
4. **Aunque funcionara en cliente, el servidor manda.** En un shooter online la
   dirección del disparo la valida el servidor. No está comprobado que aceptara
   una dirección alterada.

Lo que **sí** se implementó, y es lo que se puede sostener sin riesgo: corregir
la puntería **en el instante del disparo**, por la vía ya verificada de
`AddYawInput`/`AddPitchInput`. La diferencia práctica es que **la vista sí se
mueve**, durante un frame.

Para llegar al silent aim puro harían falta, en este orden: (1) identificar la
ruta de disparo — probablemente con un breakpoint en runtime, no estáticamente;
(2) añadir un motor de hooks con trampolín; (3) comprobar que el servidor acepta
la dirección modificada.

**Estado: parcialmente analizado, no implementado.**

Lo que ya se podría hacer solo con `GUObjectArray` (sin llamar a nada, solo
lectura, mismo nivel de riesgo que el ESP actual):
- Enumerar todos los actores del mundo sin depender de `ULevel::Actors`.
- Identificar el tipo de cualquier actor comparando su `UObject::Class` con las
  caches de `StaticClass` ya extraídas (§3) — **sin necesitar `GNames`**.
- De ahí salen ESP de objetos: armas en el suelo, cajas, vehículos, granadas.

Lo que NO se debe hacer todavía: llamar `UFunction`s. Aparte de que faltan dos
piezas, sigue en pie el problema del hilo, que es el que rompe cosas de verdad.
El orden correcto es: (1) localizar `GNames` y `ProcessEvent`, (2) encontrar un
punto de ejecución en el game thread donde encolar las llamadas, (3) probar con
una función inocua y comprobar estabilidad, y solo entonces (4) exponerlo en la UI.

---

## 10. Cómo re-derivar todo tras un parche

> **Lo que enseñó el parche del 2026-09-03**: cambiaron **todos** los RVAs y
> **ninguno** de los offsets de clase. Así que lo primero es siempre comprobar si
> los offsets siguen valiendo (rápido, por reflexión) antes de sospechar de ellos.
> Ver §12 para el procedimiento exacto y el resultado de esa comprobación.

0. **Firmas primero.** `FNamePool`, `GUObjectArray` y las `AddXInput` tienen firma
   de bytes documentada (§2, §2.3, §9.1). Búscalas con `find_bytes` y listo; no
   hace falta re-derivar nada.
1. **`GWorld`**: §2.1. No se deriva: se busca en runtime. No restar deltas.
2. **Offsets de propiedad**: buscar el nombre ASCII en `.rdata`, localizar los
   qwords alineados a 8 que lo referencian (son `FPropertyParams`), leer
   `Offset` en `+0x32` (uint16). Descartar los que tengan `CPF_Parm` (`0x80`):
   son parámetros de función, no miembros.
3. **Dueño de una propiedad**: desde el `FPropertyParams`, buscar el qword que lo
   referencia (es una entrada del array `PropPointers[]`) y expandir a los lados
   mientras los vecinos sigan pareciendo `FPropertyParams`. La lista de props
   hermanas identifica la clase sin ambigüedad.
4. **`sizeof` de una clase**: §3, extracción masiva.
5. **Sanity check obligatorio**: todo offset debe caber en el `sizeof` de su clase.
   Fue lo que delató que `PlayerArray = 0x338` era imposible.

### `FPropertyParams` (UE5)
```
+0x00 const char* NameUTF8
+0x08 const char* RepNotifyFuncUTF8
+0x10 uint64 PropertyFlags     (0x20 = CPF_Net, 0x80 = CPF_Parm, 0x2000 = CPF_Transient,
                                0x4000 = CPF_Config)
+0x18 uint32 GenFlags          (0x00 Byte/Enum, 0x03 Int32, 0x0A Float, 0x0B Double,
                                0x0C/0x4C Bool, 0x0E WeakObject, 0x11 Class, 0x12 Object,
                                0x15 Str, 0x16 Array, 0x19 Struct, 0x1E Enum,
                                0x52 Object|ObjectPtr)
+0x20 SetterFunc
+0x28 GetterFunc
+0x30 uint16 ArrayDim
+0x32 uint16 Offset            ← AQUÍ, no en +0x24
```
Para los `bool`, el mismo bloque es `FBoolPropertyParams`:
`+0x32 ElementSize`, `+0x34 SizeOfOuter` (= `sizeof` de la clase dueña, sirve de
sanity check), `+0x38 SetBitFunc` (decompilarla da byte y máscara exactos).

---

## 11. Trabajo en IDA (notas prácticas)

- `py_eval` **no conserva variables entre llamadas** y las funciones definidas no
  ven las globales del módulo. Hacer funciones autocontenidas (imports dentro) y
  cachear los buffers en un atributo de `sys` (`sys._bc = {seg: (bytes, base)}`).
- Cargar los segmentos a `bytes` y buscar con `re` es **mucho** más rápido que las
  APIs de IDA. `idautils.Strings()` da timeout en este binario.
- Los nombres de clase y de paquete están en **UTF-16**. Los nombres de propiedad,
  en **ASCII**.
- El compilador no emite `imul r, r, 0x60` para indexar `FTransform`: usa
  `lea r,[r+r*2]` + `shl r,5`. Lo mismo con `0x18` (`lea` + `shl 3`).
- Para localizar una global a la que se accede desde muchos sitios, **cuenta los
  destinos y quédate con el que gane por goleada**. `GUObjectArray` sale 814/814
  y `FNamePool` 11/11. Una global que aparece una sola vez casi siempre es ruido.
- Las firmas que llevan **la misma dirección dos veces** (los dos `lea` del bloque
  de `FNamePool`) se autovalidan: si los dos `rel32` no resuelven al mismo sitio,
  no es la firma.
- Antes de dar por bueno un `sizeof` sacado de `GetPrivateStaticClassBody`,
  **mira el desensamblado de un caso concreto**. Hay tres enteros seguidos en la
  pila y es fácil quedarse con la alineación (§3).

---

## 12. Qué comprobar cuando el juego se actualiza

Procedimiento seguido tras el parche del 2026-09-03, y resultado.

### Paso 1 — ¿cambió el binario?
```
imagebase, número de funciones y segmentos
```
394.552 → **394.579** funciones y `.text` `0x1000` más grande. Binario nuevo.

### Paso 2 — ¿se movieron los offsets de clase? (5 minutos, decide todo lo demás)
Extraer por reflexión los offsets de las props clave y compararlos con este
documento. Nombres a comprobar como mínimo:

`OwningGameInstance`, `GameState`, `LocalPlayers`, `PlayerController`,
`AspectRatioAxisConstraint`, `AcknowledgedPawn`, `PlayerCameraManager`,
`CameraCachePrivate`, `PlayerArray`, `PawnPrivate`, `PlayerNamePrivate`,
`TeamId`, `RootComponent`, `Mesh`, `CapsuleComponent`, `CapsuleHalfHeight`,
`SkinnedAsset`, `LeaderPoseComponent`, `CharacterSet`, `Health`, `MaxHealth`,
`ControlRotation`, `PlayerState`, `Controller`.

**Resultado 2026-09-03: los 24 idénticos.** Ni uno se movió. Los offsets sacados
por ASM (`ComponentToWorld@0x1D0`, `CST@0x598`, `CurrentRead@0x5E0`,
`RotationInput@0x528`, vtable slot `0x330`) también se re-verificaron: idénticos.

### Paso 3 — RVAs
**Estos sí cambian siempre.** Búscalos por firma (§2, §2.3, §9.1), no por delta:

| Qué | Firma |
|---|---|
| `FNamePool` | `74 09 4C 8D 05 ?? ?? ?? ?? EB 16 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B C0 C6 05 ?? ?? ?? ?? 01` |
| `GUObjectArray.Objects` | `48 8D 14 40 48 8B 05 ?? ?? ?? ?? 48 8B 0C C8` |
| `AddPitch/Roll/YawInput` | cola en `+0x73`: `0F 5A C0 F2 0F 58 83 <off32> F2 0F 11 83 <off32> 48 83 C4 30 5B C3` |

### Paso 4 — comprobar que el cliente arranca solo
En *Debug*:
- **Names (GNames)** debe decir "at known RVA (verified)" o "found by signature".
  Si dice "found by signature", **actualiza `kRVA_GNames_Hint`** en
  `game_names.hpp` para que la próxima vez no tenga que escanear.
- **Engine function** debe listar los dos RVAs y decir de dónde salieron. Mismo
  criterio: si salieron del escaneo, actualiza las constantes de `game_calls.hpp`.
- **World anchor / Chain** debe llegar a `World confirmed: yes`.

El cliente **funciona igual aunque no actualices las pistas**; solo se come un
escaneo de `.text` (unas decenas de ms) una vez al inyectar.

---

## 13. Pendiente

Nada de esto bloquea el ESP; son mejoras o cosas que solo se pueden cerrar en partida.

**Solo verificable jugando** — el cliente ya muestra todo esto en *Debug*
1. **Nombre de clase del pawn local** (*Debug → Names (GNames)*). Si sale un
   nombre real, `GNames` y `UObject::Class@0x10`/`Name@0x18` quedan confirmados
   de punta a punta. **Es la comprobación más valiosa que falta.**
2. `MaxHealth` real (¿100?), para escalar la barra de vida.
3. Si `POV.FOV @0x1450` cambia al apuntar con mira.
4. Que `TeamId` distinga bandos de verdad en los modos por equipos (en un modo
   libre para todos puede valer lo mismo para todos).
5. Que el filtro de huesos por nombre funcione: *Bone counters → `named X/Y`*
   debería dar `named == total`, y `body bones` rondar 20 por modelo.
6. Si con `FOV Axis` en **Auto** la proyección cuadra, ahora que `FOV Scale`
   está calibrado a 1.150. Aclararía la discrepancia del eje (§5).

7. Que el ancla del mundo se recupere sola al cambiar de mapa:
   *Debug → World anchor → `Map-change recoveries`* debería subir, y
   *Chain → `World confirmed`* volver a **yes** sin reinyectar (§2.1).

**Requiere más análisis en IDA**
8. `ProcessEvent` — **no localizado**. Es lo único que falta para poder llamar
   `UFunction`s (`GUObjectArray` §2 y `GNames` §2.2 **sí** están localizados).
   Ancla disponible: la cadena wide "Script call stack".
9. Dueño real del componente con `DeathState@0x108`. El `sizeof` corregido de
   `UBodycamSurvivorComponent` (`0x110`) encaja justo, lo que lo hace muy
   probable, pero sigue sin haber una prop reflexionada que lleve del pawn a ese
   componente. Hoy se usa `Health > 0` para saber si alguien está vivo.
10. `ABodycamSpectatorPawn` (`0x358`) deriva de `APawn`, no de `ACharacter`, pero
    `ClassifyPawn` lo clasifica como jugador porque su nombre contiene "Pawn".
    No provoca ningún fallo visible —el guardia de malla (§6) descarta su pose y
    la caja sale de la cápsula con valores acotados— pero si algún día molesta,
    la solución es tratarlo como "pawn que no es Character", igual que el dron.
11. `ULevel::Actors` para ESP de objetos del mundo: **no es UPROPERTY** en UE5,
    no sale por reflexión. Anclas: `UWorld::PersistentLevel@0x30`,
    `UWorld::Levels@0x178`. Alternativa ya disponible: recorrer `GUObjectArray`
    e identificar el tipo comparando `UObject::Class` con las caches de
    `StaticClass` extraídas (§3) — no hace falta `ULevel::Actors`.
12. Armas: `UBodycamEquipmentManagerComponent` / `UBodycamInventoryComponent`
    para un ESP de arma equipada.
13. La ruta de disparo, para el silent aim puro (§9.2).

---

## 14. Changelog

### 2026-09-04 — Parche del juego (2026-09-03): re-derivación y anti-fragilidad

El juego se actualizó y aparecieron dos fallos: el ESP se quedaba "colgado" en
`fail` hasta reinyectar, y el esqueleto se dibujaba mal. **Las dos causas eran
distintas y las dos estaban en el cliente, no en el juego.**

#### Qué cambió el parche
- Binario nuevo: **394.579 funciones** (antes 394.552), `.text` `0x1000` mayor.
- **Todos los RVAs se movieron.** Los datos por `0x1180`, el código por otra
  cantidad distinta (`0x2D0` en el caso de las `AddXInput`). No hay un delta
  único: **no se pueden restar deltas**.
- **Ni un solo offset de clase cambió.** Se re-verificaron 24 props por reflexión
  y 5 offsets por ASM: todos idénticos. También los `sizeof`. Ver §12.
- `/Script/Bodycam` pasa de 155 a **156** clases; ninguna de las relevantes cambió.

#### Fallo 1 — el ancla del mundo se quedaba pegada a un mundo muerto
**Causa raíz** (§2.1, trampa F): la validación del `UWorld` recorría
`UWorld → GameInstance → LocalPlayer → PlayerController`, pero `GameInstance` y
`LocalPlayer` **sobreviven al cambio de mapa**. Un `UWorld` ya destruido seguía
pasando la validación indefinidamente, así que el cliente leía el `GameState`
del mapa anterior y **nunca volvía a buscar**. Reinyectar era el único arreglo.

**Corrección**:
- Prueba de propiedad específica del mundo: el `PlayerArray` de su `GameState`
  tiene que contener tu propio `APlayerState`. Sin offsets nuevos.
- Recuperación inmediata por `PlayerController → Outer → ULevel::OwningWorld`,
  con re-anclaje automático. Es seguro por construcción: exige igualdad exacta
  de punteros, así que solo puede fallar en dejar de rescatar, nunca en dar un
  falso positivo.
- Red de seguridad con umbral que se duplica (120 frames → ~2 min) para que un
  caso legítimamente no confirmable no cause un tirón cada dos segundos.
- El escaneo pasa a elegir **por niveles** en vez de "el primero que valga".

#### Fallo 2 — el esqueleto se dibujaba mal
**Causa raíz**: `GNames` tenía el RVA a fuego (`0x99C1840`), que el parche movió
a `0x99C29C0`. Sin nombres válidos, **los drones dejaban de detectarse**, así que
se les leía la pose como si fueran personajes y salían líneas disparadas.

**Corrección**:
- `FNamePool` se localiza **por firma de bytes** (§2.3), con el RVA solo como
  pista rápida. Firma autovalidante (dos `lea` al mismo destino) y comprobación
  funcional (`FName` 0 = `"None"`).
- Guardia de malla (§6): se descarta la pose si la transformada no tiene sentido
  o si la malla está a más de 5 m del `RootComponent` del actor. Con contador
  `badMesh` en *Debug*.

#### Anti-fragilidad general
- `AddPitchInput` / `AddYawInput` también pasan a localizarse por firma. Sus RVAs
  nuevos son `0x3CB83C0` y `0x3CB85D0`.
- Se verificó sobre el binario nuevo, simulando el matcher byte a byte, que las
  tres búsquedas devuelven exactamente las direcciones correctas.
- El escaneo de `.text` va en bloques de 64 KB con 64 B de solape, y solo corre
  si la pista falla. Reintentos acotados (máx. 3, con 2 s de espera) para que un
  fallo no dispare un escaneo por cada nombre leído.

#### Descubrimientos nuevos
- **`UObject::Class@0x10`, `Name@0x18` y `Outer@0x20` verificados por ASM**
  (antes `[DED]`), junto con `ObjectFlags@0x08`.
- **`ULevel::OwningWorld@0xC0`** `[REFL]`+`[ASM]`, y confirmado que el `Outer` de
  un actor es su `ULevel`.
- `GUObjectArray` re-localizado (`0x9AA6170`) con firma documentada.
- Corregidos tres `sizeof` mal extraídos: `USceneComponent` `0x230` (no `0x2B8`),
  `UBodycamSurvivorComponent` `0x110` (no `0x8D0`), `UShotgunAttributeSet` `0x60`
  (no `0x130`). El de `UBodycamSurvivorComponent` refuerza que es el dueño de
  `DeathState@0x108`.
- Nueva §12 con el procedimiento de comprobación tras cada parche.

Compila con 0 errores y 0 warnings, también con `/W4`.

### 2026-09-02 — Primer análisis completo del binario
- Identificado el módulo del juego (`/Script/Bodycam`) y extraídas **5.410 clases**
  con `sizeof` y RVA de `StaticClass` (155 propias del juego).
- **`GWorld`**: se intentó derivar por RVA y fallo dos veces (`0x9934600` era
  `__security_cookie`; `0x9C1DAA0` es `GEngine`). Se sustituyó por búsqueda y
  validación en runtime, que además sobrevive a los parches (§2.1).
- Verificada la cadena completa mundo → cámara, incluido `CameraCachePrivate@0x1410`
  (está reflexionada en este binario) → `POV@0x1420`.
- **`PlayerArray` corregido a `0x2C0`**; se demostró que el `0x338` heredado no cabe
  en `AGameStateBase` (`sizeof 0x300`).
- Descubierto que la vida usa **GAS**: `Character+0x668 → UCharacterAttributeSet`,
  `Health.CurrentValue @0x94` (float). El `double @0xB0` heredado ya no aplica.
- **Equipo localizado en `ABodycamPlayerState::TeamId@0x388`** (replicado), no en el pawn.
- Sistema de huesos confirmado por ASM: `CST[i] @0x598+0x10i`, `CurrentRead@0x5E0`,
  `sizeof(FTransform)=0x60`, `GetRefSkeleton` = vtable slot `0x330`.
- `ComponentToWorld@0x1D0` confirmado por ASM (no es UPROPERTY).
- Documentados 9 bugs de lógica del código heredado (§8), incluidos el W2S sin
  rechazo de puntos detrás de la cámara y la doble liberación de `clearVariable`.
- **`GUObjectArray` localizado y verificado** (`0x9AA4FE0`), junto con el stride
  de `FUObjectItem` (`0x18`), el tamaño de chunk (65536) y
  `UObject::InternalIndex@0x0C`.

#### Reescritura del cliente (misma fecha)
- `HookFunc.h`: offsets rehechos y agrupados por clase, con el `sizeof` de cada
  una anotado para poder comprobar que todo cabe.
- `reader.hpp`: lector nuevo. Se elimina el `VirtualQuery` por lectura (era una
  syscall por hueso y por jugador) y se sustituye por validación de rango + SEH,
  que en x64 no cuesta nada mientras no salte. Se añade `ChainStage` para saber
  en cuál de los nueve eslabones falla la resolución.
- `WorldToScreen.hpp`: proyección corregida (multiplicadores de eje + rechazo de
  lo que está detrás de la cámara) y sistema de huesos nuevo: la pose se lee de
  una sola vez y la jerarquía sale del `FReferenceSkeleton`, localizado por
  escaneo validado con invariantes en vez de por índices a fuego.
- `esp_render.hpp` (nuevo): primitivas de dibujado con doble trazo, texto
  escalable y barra de vida. Separadas del resto a propósito.
- `Source.cpp`: ESP y menú reescritos; el menú pasa a barra lateral con
  secciones, buscador, interruptor maestro y una sección de diagnóstico que
  desglosa por qué se descarta cada jugador.
- **Eliminado `/FORCE:MULTIPLE` del linker.** Tapaba que `adresses` y `hooks`
  estaban duplicadas entre `Source.obj` y `hookfunc.obj`, y provocaba el aviso
  `LNK4088: image may not run`. Las globales de headers pasan a `inline`.
- Compila con 0 errores y 0 warnings, incluso subiendo el nivel a `/W4`.

#### Aimbot por función del juego + soft aim (misma fecha)
- Localizado el **sistema de input de rotación**: `RotationInput@0x528` (no es
  UPROPERTY) y las tres funciones `AddPitchInput` / `AddRollInput` /
  `AddYawInput` con sus RVAs (§9.1).
- **El aimbot pasa a alimentar `RotationInput` llamando a las funciones del
  juego.** Antes escribía `ControlRotation`, que el motor recalcula cada tick a
  partir de ese mismo `RotationInput`: por eso el método viejo peleaba con el
  juego. Se conserva como opción en el menú.
- `game_calls.hpp` (nuevo): verificación de firma en runtime (prólogo + cola con
  el offset del eje incrustado), comprobación de página ejecutable, llamada
  envuelta en SEH, tope de grados por frame y **medición automática de la escala
  de input** mediante un sondeo imperceptible. Si la firma no valida, cae solo a
  escritura directa de `RotationInput`.
- **Soft aim** añadido: corrige hacia la cabeza en el instante del disparo, con
  su propio FOV y suavizado. Se documenta explícitamente que mueve la vista y
  que el silent aim puro no es viable hoy (§9.2).
- `UWeaponAttributeSet` documentado por completo (33 atributos).

#### Correcciones tras la primera prueba en partida (misma fecha)
El diagnóstico en pantalla permitió localizar cinco problemas reales:

1. **`GWorld` se encontraba y luego se quedaba fallando para siempre.** Si el
   escaneo posterior no hallaba nada, se devolvía la global antigua, ya sabida
   inválida; como seguía siendo `!= 0`, el código se quedaba en "encontrado pero
   fallando" sin volver a buscar de verdad. Ahora se toleran 20 fallos seguidos
   (las transiciones de muerte/respawn/dron los producen), y si el escaneo no
   encuentra nada la global se pone a **0** para forzar una búsqueda limpia.
2. **`GNames` localizado** (§2.2), lo que desbloquea las dos correcciones
   siguientes.
3. **Esqueleto ilegible.** Se dibujaban los ~150 huesos del modelo: los dedos
   amontonaban líneas y los huesos de anclaje del arma salían disparados lejos
   del cuerpo. Ahora se filtra por **nombre real** del hueso y se dibujan solo
   los ~20 del cuerpo. `drawParent` guarda el ancestro dibujable más cercano,
   así que la cadena no se rompe aunque por medio haya twists o correctivos.
   Si los nombres no se pueden leer, hay un respaldo que descarta los segmentos
   de más de 60 cm (que son justamente los disparados).
4. **El ancho de la caja bailaba al correr.** Venía de usar el *bounding box*
   completo de los huesos: al mover los brazos, la caja se ensanchaba. Ahora del
   pose solo se toma el **rango vertical** y el centro horizontal; el ancho se
   deriva de la altura con proporción humana fija (alto/ancho ≈ 2.6), así que
   sigue la postura sin bailar.
5. **Vista previa de *Visuals* descuadrada**, por dos motivos distintos:
   - Alto fijo de 90 px, así que con la fuente grande el texto se salía. La
     altura pasa a calcularse a partir del tamaño de fuente.
   - `GetContentRegionAvail().x` devolvía un ancho **mayor que el área
     realmente visible**, así que la mitad clara ocupaba casi todo el recuadro y
     la muestra quedaba pegada al borde derecho, cortada. La solución es dibujar
     dentro de un `BeginChild` propio y usar `GetWindowPos()`/`GetWindowSize()`,
     que sí dan las coordenadas exactas del área y además recortan solos.

   (El fondo claro de media vista previa es intencional: sirve para ver para qué
   está el contorno negro. Ahora lleva un rótulo que lo explica.)

Añadido `FOV Scale` (§5): corrección fina de la escala de proyección, por si el
FOV publicado no coincide con el que el juego usa para la matriz. El síntoma es
que la caja se desplaza más cuanto más cerca del borde está el enemigo. Por
defecto `1.0`; en Hell Let Loose: Vietnam el valor que cuadraba era `1.330`.

#### Drones (misma fecha)
Al morir —o al desplegar uno— el jugador pasa a controlar un **dron**, así que su
pawn deja de ser un `ABodycamCharacter`. Por eso las entidades no desaparecen al
morir: siguen siendo válidas, solo cambian de clase.

Se detectan por **nombre de clase** (`UObject::Class@0x10` → `Name@0x18` → GNames),
buscando `drone` o `perk` (`ABodycamControllablePerk`, y existe
`UDroneMovementComponent`). El resultado se cachea **por `UClass`**, no por objeto.

Dos consecuencias importantes de que un dron derive de `APawn` y no de `ACharacter`:
- **No tiene `CapsuleComponent` en `0x338`** — ahí hay otra cosa. Leerlo daría
  medidas sin sentido, así que para drones se usa un tamaño fijo.
- **No tiene `CharacterSet` en `0x668`** (cae fuera de la clase), así que no se
  les lee la vida.

Se marcan en pantalla con la etiqueta `[DRONE]` y tienen su propio filtro.

#### Segunda auditoría de robustez (misma fecha)
Revisión del código añadido para nombres, drones y búsqueda del mundo. Cuatro
fallos encontrados y corregidos:

1. **Reintento de nombres en bucle.** Si un modelo no daba nombres utilizables,
   el filtro del esqueleto se reconstruía **cada frame y para cada jugador**,
   releyendo ~100 `FName` cada vez. Limitado a 3 intentos por asset.
2. **El aimbot podía apuntar a un dron.** Un dron no es un `ACharacter`, así que
   leerle la pose devolvía posiciones inventadas y el aimbot apuntaba a la nada.
   Ahora se descartan como objetivo, con su propio contador en el log.
3. **El escaneo del mundo se repetía dos veces por segundo** cuando no había
   mundo (menú principal). Cada pasada cuesta ~170 ms, así que se comía los FPS
   del overlay sin ganar nada. Añadida espera progresiva: 500 ms doblando hasta
   5 s mientras siga sin aparecer, y vuelta al mínimo en cuanto se encuentra.
4. **`Detour32` escribía 8 bytes donde caben 4.** Un `jmp rel32` lleva un
   desplazamiento de 4 bytes; escribir un `uintptr_t` entero machacaba 4 bytes
   de más del código del juego. Es código muerto (ninguna función de hook se usa
   hoy), pero corregirlo evita una bomba de relojería si se retoma para el hook
   del §9.2. Se le añadió además la comprobación de alcance de ±2 GB y el
   `FlushInstructionCache`.

Verificado también que los punteros devueltos por el caché de esqueletos se usan
siempre dentro de la misma iteración, sin que medie otra inserción que pudiera
invalidarlos.

#### Auditoría de robustez (misma fecha)
Revisión línea a línea buscando fallos que solo aparecerían en ejecución. Seis
encontrados y corregidos:

1. **`createDirectX()` sin comprobar** — si fallaba (sistema sin D3D9, driver
   ocupado, sin recursos), `pDevice` quedaba nulo y el primer frame hacía
   `pDevice->Clear(...)` sobre un nulo: **crash del juego**, no del overlay.
   Ahora se comprueban `createOverlay()`, `createDirectX()` y `pDevice`, y ante
   un fallo la DLL se descarga sin tocar nada.
2. **`read<T>` pone el destino a CERO al fallar**, no conserva el valor previo.
   Se leía `TeamId` con `int teamId = -1;` dando por hecho que un fallo dejaría
   el `-1`; en realidad dejaba `0`, que es un equipo válido. Un fallo de lectura
   podía marcar a un enemigo como aliado y ocultarlo. Corregido en los tres
   sitios (ESP, aimbot y `ReadValues`), comprobando el retorno.
3. **Trabajo pesado en `DllMain`** — reservas de memoria, `std::string` y
   llamadas a `user32` bajo el *loader lock* del proceso. Es una fuente clásica
   de bloqueo del juego entero. Movido a `InitOnThread()`, que corre ya en el
   hilo; `DllMain` solo crea el hilo.
4. **`ImGui_ImplDX9_Shutdown()` incondicional** al salir, aunque nunca se
   hubiera inicializado ImGui. Ahora va bajo `if (isInitialized)`.
5. **`Reset()` del dispositivo sin invalidar los objetos de ImGui** al cambiar el
   tamaño de la ventana: la pasada siguiente dibujaba con recursos muertos.
6. **Calibración de escala envenenable**: si el tick del juego consumía
   `RotationInput` justo entre las dos lecturas, la medida era basura y podía
   corromper la escala. Ahora se rechazan cambios de signo y saltos de más de
   3× sobre la escala ya medida, y no se calibra si alguna lectura falló.

Además: el aimbot y el soft aim quedan en pausa con el menú abierto (si no, cada
clic en un control disparaba el soft aim), y se eliminó `GetBoneWorld`, que era
código muerto que invitaba a releer la pose entera por cada hueso.
