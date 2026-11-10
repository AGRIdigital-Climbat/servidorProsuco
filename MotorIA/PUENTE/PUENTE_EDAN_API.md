# API Puente para el asistente de llenado EDAN

> Especificación del contrato que necesita el asistente conversacional EDAN para
> funcionar sobre `POST /puente` y `POST /puente/stream` del MotorIA v2.3.0.
>
> Base de esta especificación:
>
> - La spec viva del puente (`GET /openapi.json`).
> - El código de los dos clientes:
>   - `REA/lib/services/edan_ia/edan_groq_service.dart` (transporte y validación del parche)
>   - `REA/lib/services/edan_ia/edan_groq_prompt.dart` (instrucción de sistema)
>   - `REA/lib/services/edan_ia/edan_estado_asistente.dart` (bloque de estado)
>   - `EDAN-Tutor/src/services/puente.js` y `prompt.js` (cliente web)
> - 4 corridas reales contra `openai/gpt-oss-20b` mediante
>   `REA/tool/prueba_groq_edan.dart`.

Todo lo que sigue está medido, no supuesto. Donde dice «se midió» hay una
corrida real detrás.

---

## Índice

1. [Por qué no alcanza el contrato actual](#1-por-qué-no-alcanza-el-contrato-actual)
2. [Nombres de campo: `instruccion`, no `sistema`](#2-nombres-de-campo-instruccion-no-sistema)
3. [`PuenteRequest` v2](#3-puenterequest-v2)
4. [Respuesta sin streaming](#4-respuesta-sin-streaming)
5. [SSE para `/puente/stream`](#5-sse-para-puentestream)
6. [Errores tipificados](#6-errores-tipificados)
7. [Límite de tokens](#7-límite-de-tokens-la-restricción-que-condiciona-todo)
8. [El parche EDAN: contrato completo](#8-el-parche-edan-contrato-completo)
9. [Ejemplo completo de un turno](#9-ejemplo-completo-de-un-turno)
10. [Degradación con modelo local](#10-degradación-con-modelo-local)
11. [Responsabilidades](#11-responsabilidades)
12. [Migración del cliente web](#12-migración-del-cliente-web)
13. [Pruebas de aceptación](#13-pruebas-de-aceptación-del-puente)
14. [Anexo: esquemas JSON](#14-anexo-esquemas-json)

---

## 1. Por qué no alcanza el contrato actual

El puente ya resuelve dos cosas bien:

- **Custodia de la clave.** Hoy `ApiConfig.groqApiKey` está hardcodeada en el
  repo y viaja dentro del APK. En el puente la clave está cifrada con AES-256-GCM
  y nunca sale del servidor.
- **Rotación de proveedor.** `llm_externo: true` sortea entre los proveedores
  habilitados. Es exactamente lo que hace falta cuando se usan APIs gratuitas
  y una se queda sin cuota. **Este sorteo se mantiene a propósito.**

Lo que no puede es transportar lo que el llenado EDAN necesita:

| # | Lo que falta | Consecuencia medida en campo |
|---|---|---|
| 1 | No declara `response_format` | Al ser un modelo Pydantic, los campos desconocidos se descartan: no hay forma de pedir JSON. Se midió a `gpt-oss-20b` respondiendo en **prosa en 1 de cada 3 turnos**. El provider descartaba el turno entero: la persona hablaba y sus datos se perdían. |
| 2 | No declara `razonamiento` | Los modelos razonadores vuelcan su cadena de pensamiento dentro del texto. El Tutor web ya sufre esto con los Nemotron: quemaban 800 tokens y dejaban el razonamiento en inglés en la respuesta. |
| 3 | `max_tokens` con default 512 | Trunca el parche a mitad de camino. El resultado es un JSON inválido que el cliente ve como «el modelo no entendió». |
| 4 | No tipifica los errores | Un `429` de Groq (límite de 8.000 TPM) llega crudo. El cliente no puede decidir si reintenta o si baja al extractor determinista. |
| 5 | Respuesta sin schema | El OpenAPI declara `{}` en el `200`. El Tutor tuvo que escribir `normalizarRespuesta` a la defensiva porque `respuesta` a veces llegaba como objeto y la UI pintaba `[object Object]`. |
| 6 | Sin reintento de formato | Cuando el modelo conversa largo y deja el parche vacío, no hay segunda pasada. Se midió: el modelo respondía «Entiendo que el camino a San Pedro se rompió. ¿Cuánto mide el tramo?» y la calle se perdía. |

Con `response_format: {"type": "json_object"}` reenviado al proveedor, el mismo
modelo pasó **5 de 5** intentos. Ese es el cambio que más importa de todos los
de esta lista.

---

## 2. Nombres de campo: `instruccion`, no `sistema`

El puente tiene hoy tres campos que se solapan:

| Campo | Qué hace hoy | Problema |
|---|---|---|
| `prompt` | «Instrucción a enviar al modelo tal cual» | El cliente web documentó que el servidor lo ignora y mete su propio system prompt. |
| `sistema` | «System prompt propio. Por defecto usa el genérico del puente» | Cuando viene vacío, el puente inyecta el suyo. Reutilizarlo para EDAN vuelve ambiguo de dónde salió la instrucción. |
| `mensajes` | Multi-turno | Correcto. Se queda. |

**Requisito: campo nuevo `instruccion`.** Es aditivo, no rompe a nadie y tiene
precedencia sobre `sistema`. `sistema` y `prompt` se conservan intactos para no
romper el asistente legal, que los usa con otro sentido.

Orden de precedencia que debe aplicar el puente:

```text
instruccion  >  sistema  >  prompt genérico del puente
```

El cliente EDAN manda siempre la instrucción completa y ya armada, con el bloque
de estado incluido, porque el prompt se queda en el cliente (ver §11).

---

## 3. `PuenteRequest` v2

```jsonc
{
  // ---- conversación (nuevo: instruccion) --------------------------
  "instruccion": "Sos quien ayuda a llenar el formulario EDAN…",  // string, nueva
  "mensajes": [
    { "role": "user",      "content": "se desbordó el río en Yapacaní" },
    { "role": "assistant", "content": "¿Cuántas familias se vio afectada?" }
  ],
  "prompt": null,                                                 // ya existe

  // ---- garantía de formato (nuevo) --------------------------------
  "formato": "json",          // "texto" (default) | "json"
  "json_schema": null,        // object opcional: valida y reintenta
  "intentos_json": 1,         // int 0..2, default 1

  // ---- control de modelo (nuevo) ----------------------------------
  "razonamiento": "off",      // "off" | "low" | "medium" -> reasoning_effort
  "stop": ["\n\n"],           // string[]

  // ---- ya existen, con los valores que EDAN necesita -------------
  "max_tokens": 1200,
  "temperatura": 0.3,
  "llm_externo": true
}
```

### 3.1 Tabla de campos

| Campo | Tipo | Req | Default | Qué debe hacer el puente |
|---|---|---|---|---|
| `instruccion` | string | no | null | Va como mensaje `system`. Precede a `sistema` |
| `mensajes` | `[{role, content}]` | no | `[{user, "Quiero reportar un evento."}]` | Historial literal, sin reescribir ni resumir |
| `prompt` | string | no | null | Atajo de un solo turno. Si viene `mensajes`, gana `mensajes` |
| `formato` | enum | no | `"texto"` | `"json"` ⇒ reenviar **`response_format: {"type":"json_object"}`** al proveedor **y verificar que lo que vuelve parsea** |
| `json_schema` | object | no | null | Si viene, valida `json` contra el schema. Si falla, cuenta como intento fallido |
| `intentos_json` | int | no | `1` | Intentos extra cuando `formato:"json"` y no sale JSON válido. Tope 2 |
| `razonamiento` | enum | no | `"off"` | Mapea a `reasoning_effort`. `"off"` = el proveedor no razona |
| `stop` | string[] | no | null | Se pasa tal cual. El Tutor lo pidió para cortar tablas markdown |
| `max_tokens` | int | no | 512 | **Para EDAN el cliente manda 1200–4096.** Ver §7 |
| `temperatura` | number | no | 0.7 | Para EDAN: 0.3 |
| `llm_externo` | bool \| null | no | modo global | Se mantiene el sorteo aleatorio entre proveedores habilitados |

### 3.2 Qué se manda en el intento de reparación

El cliente ya tiene una versión de esto implemented y funciona
(`EdanIaBorradorProvider._reparar`). El puente debe replicar la misma forma, no
inventar otra:

```text
instruccion: instrucción corta de extracción (ver edanGroqInstruccionReparacion)

user:        Lo que dijo la persona: "También se rompió el camino a San Pedro"
             Anotá lo que ya dijo, sin preguntar nada.
             Si de verdad no hay ningún dato, devolvé el parche vacío.
```

Salida esperada: `{"parche": {...}}` o `{"parche": {}}`.

**Máximo una reparación por turno.** Reintentar sin parar es lo que tumbó la
primera corrida real por límite de tokens.

---

## 4. Respuesta sin streaming

Hoy el `200` no declara nada. Debe quedar así:

```json
{
  "respuesta": "Lamento mucho lo de Yapacaní. ¿Cuántas familias quedaron con su vivienda dañada?",
  "json": {
    "parche": {
      "municipio": "Yapacaní",
      "totalAfectada": 350,
      "tipoEvento": { "inundacion": true },
      "infraVial": [
        { "via": "Ruta 1", "tramo": "cerca del hospital", "danoTotal": "Total" }
      ]
    }
  },
  "formato": "json",
  "modelo": "openai/gpt-oss-20b",
  "proveedor": "Groq",
  "intentos": 1,
  "tokens": { "entrada": 2640, "salida": 212, "razonamiento": 0 },
  "avisos": []
}
```

### Reglas del envelope

1. **`respuesta` es SIEMPRE string.** Aunque el proveedor devuelva un objeto, se
   serializa. Nunca `[object Object]`.
2. **`json`** lleva el objeto ya parseado, para que el cliente no lo vuelva a
   parsear ni tenga que hacerlo con regex sobre texto.
3. **`formato`** dice cómo llegó la respuesta real. Si se pidió `"json"` y no se
   pudo, es `"texto"` y hay un aviso (§6).
4. **`intentos`** siempre entre 1 y 3. Si fue 3, algo va mal y el cliente debe
   avisarle a la persona.
5. **`avisos`** es texto para logs, no para mostrar. Valores sugeridos:
   `json_no_soportado`, `truncado_por_max_tokens`, `reintento_por_json_invalido`,
   `respuesta_no_string`.
6. **`modelo` y `proveedor` siempre presentes.** Con sorteo aleatorio, sin esto
   no se puede depurar cuál modelo rompió el contrato.

---

## 5. SSE para `/puente/stream`

```text
data: {"tipo":"token","texto":"Lamento mucho"}
data: {"tipo":"token","texto":" lo de Yapacaní."}
data: {"tipo":"razonamiento","texto":"la persona dijo que hubo inundación"}
data: {"tipo":"json","json":{"parche":{"municipio":"Yapacaní","totalAfectada":350}}}
data: {"tipo":"done","formato":"json","modelo":"openai/gpt-oss-20b","proveedor":"Groq","intentos":1,"tokens":{"entrada":2640,"salida":212,"razonamiento":96},"avisos":[]}
data: [DONE]
```

| Evento | Cuándo | Notas |
|---|---|---|
| `token` | Cada delta de contenido | **Solo contenido visible.** Nunca razonamiento |
| `razonamiento` | Delta de razonamiento | Evento propio, para que el cliente lo oculte. Flutter ya tiene `EdanGroqFragmento.esRazonamiento` |
| `json` | Una vez, al terminar | El objeto parseado. Evita que el cliente parsee texto parcial |
| `done` | Al terminar bien | Lleva `formato`, `modelo`, `proveedor`, `intentos`, `tokens`, `avisos` |
| `error` | Fallo | Payload en §6 |
| `[DONE]` | Terminador | Se mantiene por compatibilidad |

En streaming, `respuesta` se reconstruye concatenando los `token`. El evento
`json` es el que garantiza integridad: el cliente puede pintar el texto mientras
llega, pero solo aplica el parche cuando llega `json`.

---

## 6. Errores tipificados

```json
{
  "tipo": "error",
  "codigo": "rate_limit",
  "mensaje": "El proveedor alcanzó su límite de tokens por minuto",
  "reintentable": true,
  "esperar_ms": 12000,
  "detalle": { "limite_tpm": 8000, "usado": 6045, "pedido": 2632 }
}
```

| `codigo` | HTTP | `reintentable` | Qué hace el cliente |
|---|---|---|---|
| `rate_limit` | 429 | true | Reintenta una vez tras `esperar_ms`, con la ficha intacta |
| `json_invalido` | 422 | false | **Baja al extractor determinista.** Reintentar no ayuda |
| `truncado` | 422 | false | Sube `max_tokens` y avisa. Los datos pueden estar a medias |
| `timeout` | 504 | true | Una vez más, luego mensaje de red |
| `proveedor_caido` | 502 | true | El puente ya sorteó otro; el cliente solo reintenta |
| `no_autorizado` | 401 | false | Falta `X-API-Key` |
| `contexto_demasiado` | 413 | false | Acortar el historial desde el cliente |

Esta tabla es lo que hace decidible la cadena de respaldos. Hoy todo llega como
error genérico y el cliente no puede distinguir «reintentar» de «bajar al
extractor», que son decisiones opuestas.

---

## 7. Límite de tokens: la restricción que condiciona todo

El tier gratuito de Groq es de **8.000 TPM** (tokens por minuto). Medición real
del prompt EDAN:

| Parte | Tokens (aprox) |
|---|---|
| `instruccion` base (rol + reglas + vocabulario cerrado) | ~1.400 |
| Bloque de estado (`estadoParaAsistente`) | ~300–700 |
| Historial de los últimos turnos | ~200–500 |
| **Total por turno** | **~2.500** |

Eso da **2–3 turnos por minuto** por proveedor. Consecuencias que el puente debe
respetar:

- `intentos_json` máximo 1 por turno. Reintentar sin parar es lo que tumbó la
  primera corrida.
- La pausa entre turnos la pone el **cliente** (ya lo hace: 12 s).
- Con `llm_externo: true` y varios proveedores, la cuota se reparte sola. Esa es
  la razón por la que el sorteo se mantiene.
- El `max_tokens` default de 512 **no sirve**. Un parche con `infraVial` e
  `impactoPoblacion` pasa de 500 tokens. El cliente EDAN debe mandar ≥ 1200;
  Flutter usa 4096.

---

## 8. El parche EDAN: contrato completo

Esto es lo que el modelo debe devolver dentro de `json.parche`. Derivado de
`EdanGroqService.parcheDesdeJson` (`REA/lib/services/edan_ia/edan_groq_service.dart:143`).

Esquema legible por máquina: [`schemas/edan-parche.schema.json`](schemas/edan-parche.schema.json).

### 8.1 Escalares

```json
{
  "departamento": "Santa Cruz",
  "provincia": "Ichilo",
  "municipio": "Yapacaní",
  "alcalde": "Juan Pérez",
  "telefono": "70012345",
  "fechaLlenado": "2026-09-28",
  "fechaEvento": "2026-09-27",
  "horaEvento": "03:15",
  "tipoEventoOtro": "crecida del río",
  "responsableNombre": "María López",
  "responsableCargo": "Defensa Civil",
  "responsableTelefono": "70123456",
  "responsableEmail": "maria@defensa.bo",
  "totalAfectada": 350,
  "totalFamiliasAfectadas": 80,
  "totalFamiliasDamnificadas": 40,
  "viviendasAfectadas": 120,
  "viviendasDamnificadas": 55
}
```

Reglas de conversión:

- Fechas `AAAA-MM-DD`, horas `HH:MM` en 24 h.
- Nombres y lugares: **texto estricto**. Un número suelto nunca debe caer en
  `municipio`.
- Números: se acepta `350` o `"350 personas"`. Un `0` explícito **sí** cuenta
  como dato.
- `""` o solo espacios significa «no lo dijo» y se descarta. Nunca pisa lo que
  ya estaba anotado.

### 8.2 Mapas con vocabulario cerrado

Cualquier etiqueta fuera del vocabulario se **ignora** (se compara sin tildes).
Nunca se propaga un dato inválido.

**`tipoEvento`** — 7 claves canónicas, valores booleanos:

```json
{
  "tipoEvento": {
    "inundacion": true, "riada": true, "deslizamiento": false,
    "helada": false, "granizada": false, "sequia": false, "incendios": false
  }
}
```

Claves válidas: `inundacion`, `riada`, `deslizamiento`, `helada`, `granizada`,
`sequia`, `incendios`.

**`impactoPoblacion`** — la clave es la etiqueta del grupo, y solo existen cuatro:

```json
{
  "impactoPoblacion": {
    "Niño(a) 0-5 años":  { "fallecidos": 0, "heridos": 120, "desaparecidos": 2, "observaciones": "en la escuela" },
    "Escolar 6-18 años": { "fallecidos": 0, "heridos": 45,  "desaparecidos": 0 },
    "Adulto 19-60 años": { "fallecidos": 1, "heridos": 30,  "desaparecidos": 1 },
    "Adulto mayor 60+":   { "fallecidos": 2, "heridos": 18,  "desaparecidos": 0 }
  }
}
```

Etiquetas válidas: `Niño(a) 0-5 años`, `Escolar 6-18 años`, `Adulto 19-60 años`,
`Adulto mayor 60+`.

**`proteccion`** — 7 grupos:

```json
{
  "proteccion": {
    "Niñas y Niños":              { "mujeres": 20, "hombres": 25, "total": 45, "observaciones": "" },
    "Mujeres Embarazadas":        { "mujeres": 12, "hombres": null, "total": 12 },
    "Personas con Discapacidad":  { "mujeres": 3,  "hombres": 3,  "total": 6 }
  }
}
```

Etiquetas válidas: `Niñas y Niños`, `Adolescentes`, `Mujeres Lactantes`,
`Mujeres Embarazadas`, `Adultos Mayores`, `Personas con Discapacidad`,
`Enfermedad Inhabilitante`.

> Solo se leen `mujeres`, `hombres`, `total` y `observaciones`. El formulario web
> tiene además `apoyoPsicosocial` y `reposicionDoc`; **la API no los acepta
> todavía.** Cuando se agreguen al modelo, hay que sumarlos aquí y al esquema.

**`necesidadesHumanitarias`** — 10 ítems, booleanos:

```json
{
  "necesidadesHumanitarias": {
    "Alimentos": true, "Tanques de Agua": true, "Politubos": false
  }
}
```

Etiquetas válidas: `Alimentos`, `Refugio y Vivienda`, `Ropa de Cama`,
`Artículos de Higiene`, `Artículos de Cocina`, `Tanques de Agua`,
`Pastillas Potabilizadoras`, `Filtros Vela`, `Politubos`, `Carro Cisterna`.

**`primeraRespuesta`** — 3 equipos, booleanos:

```json
{ "primeraRespuesta": { "Búsqueda": true, "Salvamento/Rescate": true, "Evacuación": false } }
```

Etiquetas válidas: `Búsqueda`, `Salvamento/Rescate`, `Evacuación`.

**`necesidadSalud`** — solo texto:

```json
{
  "necesidadSalud": {
    "incendio": "", "inundacion": "medicamentos", "sequia": "", "otras": "",
    "observaciones": "hay dos puestos de salud abiertos"
  }
}
```

**`serviciosBasicos`** — 5 claves, campos de texto:

```json
{
  "serviciosBasicos": {
    "luz":            { "danoParcial": "sin energía 5 días", "danoTotal": "", "necesidades": "generador", "observaciones": "" },
    "agua":           { "danoParcial": "sin agua", "danoTotal": "", "necesidades": "cisternas", "observaciones": "" },
    "alcantarillado": { "danoParcial": "tapado", "danoTotal": "", "necesidades": "maquinaria", "observaciones": "" },
    "gas":            { "danoParcial": "", "danoTotal": "sin gas", "necesidades": "", "observaciones": "" },
    "telecom":        { "danoParcial": "sin señal", "danoTotal": "", "necesidades": "radio", "observaciones": "" }
  }
}
```

Claves válidas: `luz`, `agua`, `alcantarillado`, `gas`, `telecom`. Se mapean
internamente a `energia_electrica`, `agua_potable`, `alcantarillado`,
`gas_domiciliario` y `telecomunicaciones`.

### 8.3 Listas 1:N

```json
{
  "infraVial": [
    { "via": "Ruta 1", "tramo": "cerca del hospital", "danoParcial": "", "danoTotal": "Total",
      "kmDanio": "2", "necesidades": "maquinaria", "observaciones": "se cayó el puente" },
    { "via": "Camino a San Pedro", "tramo": "km 12", "danoParcial": "parcial", "danoTotal": "",
      "kmDanio": "8", "necesidades": "", "observaciones": "" }
  ],
  "infraPublica": [
    { "comunidad": "Santa Fe", "tipoEstructura": "escuela", "danoParcial": "techo caído",
      "danoTotal": "", "necesidades": "lonas", "observaciones": "" }
  ],
  "infraProductiva": [
    { "tipo": "silo", "comunidad": "Yapacaní", "danoParcial": "", "danoTotal": "Total",
      "necesidades": "maquinaria", "observaciones": "" }
  ],
  "saludDanios": [
    { "establecimiento": "Centro de salud Yapacaní", "comunidad": "", "nivel": "primer nivel",
      "dano": "sin agua", "observaciones": "" }
  ],
  "educacion": [
    { "establecimiento": "Escuela San Pedro", "comunidad": "San Pedro", "danoParcial": "dos aulas",
      "danoTotal": "", "suspensionClases": true, "estudiantes": 300, "observaciones": "" }
  ],
  "danosVivienda": [
    { "comunidad": "Santa Fe", "danoParcial": 40, "danoTotal": 15,
      "necesidades": "lonas y clavos", "observaciones": "" }
  ],
  "agricultura": [
    { "cultivo": "Maíz", "hasAfectadas": 120, "hasPerdidas": 30,
      "necesidades": "semillas", "observaciones": "" }
  ],
  "pecuario": [
    { "especie": "Vacas", "animalesAfectados": 40, "animalesMuertos": 12,
      "necesidades": "forraje", "observaciones": "" }
  ],
  "comunidades": [
    { "nombre": "Santa Fe" },
    { "nombre": "San Pedro" }
  ]
}
```

**Regla de todas las listas:** una fila sin ningún dato reconocible **no se
agrega**. Es preferible perder una fila inútil que ensuciar la ficha.

### 8.4 Claves prohibidas

Cualquier clave fuera de las de arriba se descarta en silencio. En particular
**nunca** se acepta: `id_seccion`, `sincronizado`, `editado`, `id_local`,
`id_edan_localidad`, `fecha_creacion`, `intentos_sync`, `ultimo_error`. Los pone
el sistema al guardar.

---

## 9. Ejemplo completo de un turno

### Request

```json
{
  "instruccion": "Sos quien ayuda a llenar el formulario EDAN…\n\n# Lo que hay anotado exactamente\n…\n**No se lo leas en voz alta ni lo resumas.** La persona ya sabe todo esto porque lo acaba de decir: repetirselo es sobrarle. Este bloque es tu memoria, no un texto para mostrar.\n…",
  "mensajes": [
    { "role": "user", "content": "se desbordó el río en Yapacaní, son 350 personas" }
  ],
  "formato": "json",
  "intentos_json": 1,
  "razonamiento": "off",
  "max_tokens": 1200,
  "temperatura": 0.3,
  "llm_externo": true
}
```

### Response

```json
{
  "respuesta": "Lamento mucho lo de Yapacaní. ¿Cuántas familias quedaron con su vivienda dañada?",
  "json": {
    "parche": {
      "municipio": "Yapacaní",
      "totalAfectada": 350,
      "tipoEvento": { "inundacion": true },
      "infraVial": [
        { "via": "Ruta 1", "tramo": "cerca del hospital", "danoTotal": "Total" }
      ]
    }
  },
  "formato": "json",
  "modelo": "openai/gpt-oss-20b",
  "proveedor": "Groq",
  "intentos": 1,
  "tokens": { "entrada": 2640, "salida": 212, "razonamiento": 0 },
  "avisos": []
}
```

### Qué hace el cliente con eso, en orden

1. Valida `json.parche` contra el esquema. Clave desconocida se ignora, no rompe.
2. Aplica el parche al borrador con fusión por clave natural. **Nunca pisa un
   dato existente**: un campo ausente en el parche no existe para el merge.
3. Recalcula progreso y pendientes de sección.
4. Corre las reglas de población. Si la suma de grupos etarios supera el total,
   genera un aviso para que el modelo lo mencione en su respuesta.
5. Habla `respuesta`. Si viene relleno tipo «Entiendo, gracias por contarme lo
   que pasó», lo descarta y queda solo la pregunta.
6. Si la ficha quedó completa y no se mencionó fotos, cierra el paso de fotos.

### Ejemplo de degradación: la API no pudo dar JSON

```json
{
  "respuesta": "Lamento mucho lo de Yapacaní. ¿Cuántas familias quedaron con su vivienda dañada?",
  "json": null,
  "formato": "texto",
  "modelo": "qwen2.5-3b-local",
  "proveedor": "local",
  "intentos": 2,
  "tokens": { "entrada": 2610, "salida": 88, "razonamiento": 0 },
  "avisos": ["json_no_soportado", "reintento_por_json_invalido"]
}
```

El cliente muestra la respuesta (es buena) y baja al extractor determinista sobre
el texto de la persona para recuperar los datos. La ficha no se rompe.

---

## 10. Degradación con modelo local

Como el sorteo se mantiene, el puente puede caer en el modelo local
(Qwen2.5-3B), que **no acepta** `response_format`. Requisito:

```text
el proveedor no soporta response_format
        ↓
degradar: validar-y-reintentar (intentos_json)
        ↓
si aun así no sale JSON → 200 con formato:"texto" y avisos:["json_no_soportado"]
        ↓
el cliente baja al extractor determinista
```

**Nunca** un 500 por esto. Si el puente está caído, la ficha se sigue llenando:
esa es la propiedad que hizo aceptable depender de APIs gratuitas. El extractor
(`edan_extractor.dart` en Flutter, `extractor.js` en el Tutor) es la última
línea de defensa y no necesita internet.

---

## 11. Responsabilidades

| Hace el puente | Hace el cliente |
|---|---|
| Custodiar y cifrar la clave del proveedor | Armar `instruccion` con el bloque de estado |
| Sortear proveedor y reintentar si uno cae | Aplicar el parche con fusión por clave natural |
| Garantizar que vuelva JSON, o avisar que no | Extraer datos de forma determinista |
| Contar tokens por llamada | Validar las reglas de población |
| Normalizar los errores | Persistir en SQLite |
| — | Capturar fotos y coordenadas con GPS |

**El puente no debe conocer el vocabulario EDAN.** Se queda genérico: si mañana
hay otro asistente, reusa el mismo endpoint sin cambios.

---

## 12. Migración del cliente web

Dos cambios, ambos en `EDAN-Tutor/src/services/puente.js`:

1. **`PUENTE_URL` por defecto a `/puente`, no a `/chat`.** Hoy la línea 28 tiene
   `import.meta.env?.VITE_PUENTE_URL || 'http://localhost:8077/chat'`. La
   variable se llama «puente» pero el default apunta a `/chat`, que es el
   endpoint con RAG legal y cuyo `ChatRequest` exige `pregunta` y **no** acepta
   `sistema` ni `mensajes`. Solo funciona si `VITE_PUENTE_URL` está puesto a mano
   en cada despliegue.
2. **`sistema:` → `instruccion:`**, y agregar `formato: 'json'` si el Tutor
   migra al modo parche.

`normalizarRespuesta` puede quedarse como red de seguridad, pero deja de ser
obligatorio una vez que `respuesta` sea siempre string y `json` venga parseado.

---

## 13. Pruebas de aceptación del puente

| # | Prueba | Criterio |
|---|---|---|
| 1 | Conversación de 20 turnos con `formato:"json"` | 20/20 con `json.parche` parseable |
| 2 | `max_tokens: 300` con un parche largo | `422 truncado`, no JSON corrupto dentro de un 200 |
| 3 | `intentos_json: 1` contra un modelo que no devuelve JSON | 1 reintento, luego `formato:"texto"` con aviso |
| 4 | Proveedor devuelve 429 | `rate_limit`, `reintentable: true`, `esperar_ms` presente |
| 5 | `razonamiento: "off"` con modelo razonador | Cero tokens de razonamiento en la respuesta |
| 6 | Sorteo con 3 proveedores | Los 3 responden, y `modelo`/`proveedor` siempre informs |
| 7 | Modo local (Qwen) | No rompe: degrada según §10 |
| 8 | `instruccion` y `sistema` juntos | Gana `instruccion` |
| 9 | Proveedor devuelve `respuesta` como objeto | El envelope lo convierte a string, nunca `[object Object]` |
| 10 | Sin `X-API-Key` | `401` con `codigo: "no_autorizado"` |

La sonda `REA/tool/prueba_groq_edan.dart` sirve de arnés. Mide anotación útil por
turno, turnos perdidos, recitación, palabras de robot, preguntas por turno y
latencia. Con solo cambiarle la URL y el `fetch` por SSE, corre contra el puente
y mide lo mismo.

---

## 14. Anexo: esquemas JSON

| Archivo | Para qué |
|---|---|
| [`schemas/puente-request.schema.json`](schemas/puente-request.schema.json) | Validar el body que manda el cliente a `/puente` y `/puente/stream` |
| [`schemas/edan-parche.schema.json`](schemas/edan-parche.schema.json) | Validar `json.parche` antes de aplicarlo al borrador |
| [`schemas/puente-respuesta.schema.json`](schemas/puente-respuesta.schema.json) | Validar el envelope que devuelve el puente |

`additionalProperties: false` en el parche es intencional: una clave desconocida
debe rechazarse en el esquema para que nadie la tome por buena. El cliente, en
cambio, **ignora** lo que no reconoce en vez de fallar, porque una respuesta
válida con una clave de más no puede_costarle la ficha a la persona.
