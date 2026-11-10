# 09 — Base de conocimiento agronómico (quinua, altiplano boliviano)

> **Este documento es la fuente de las reglas del agente.** Cada parámetro lleva su
> referencia; las reglas de [06-agentes-riego.md](06-agentes-riego.md) citan este archivo.
> Revisado con fuentes verificadas en internet el **2026-10-22** (URLs al final, §11).

---

## 1. Para qué sirve y cómo se usa

| Uso | Quién | Cómo |
|---|---|---|
| Reglas de decisión (R1–R9) | Skill del agente `agro-riego` | Tabla §10: regla → parámetro → fuente |
| Umbrales por defecto | `field_config` (doc 05/08) | §7–§8: MAD, CC/PMP, duración |
| Fase fenológica y Kc | Cálculo ETc | §4–§6 |
| Alertas (helada/calor) | `agro-alertas` | §3, §5 |
| Advertencias de interpretación | Tono del agente | §9 |

> **Honestidad de datos:** Agrisignal y esta base son **estimaciones para decisión y
> planificación**, no consejo agronómico certificado; el agente siempre muestra los
> números y su fuente (§9).

---

## 2. Ambiente del altiplano boliviano

| Parámetro | Valor | Fuente |
|---|---|---|
| Altitud | ~3 600–4 300 msnm (media ~4 000 m) | FAO *Report on the soils of Bolivia*; Vacher & Brasier (IRD) |
| Régimen | Semiárido–árido, alta demanda evaporativa, suelos pobres, heladas y sequías frecuentes | Simulación AquaCrop (SciELO Bolivia) |
| Precipitación año (norte/centro) | **404–800 mm** (año normal, 50 %) | AquaCrop altiplano (SciELO Bolivia) |
| Precipitación año (sur/intersalar) | **156–400 mm**; altiplano sur 150–300 mm; promedio SENAMHI ~237 mm | AquaCrop (SciELO Bolivia); Winkel 2015; PMC 8624588 |
| Concentración de lluvias | Temporada estival austral (~nov–mar), irregular y con déficit hídrico real (P < ETa) | SciELO Chile; SciELO Bolivia |
| Frío | Condiciones frías desde 3 500–4 000 m; heladas invernales frecuentes (mín. < 0 °C, máx. 16–21 °C en frío) | FAO suelos Bolivia; SENAMHI/RedUno (jul 2026) |
| Heladas en el cultivo | **Noche con helada puede ocurrir durante todo el ciclo** (radiación extrema + heladas nocturnas) | Winkel/Pouteau vía CONICET 2012 |
| Viento | 14,4–43,2 km/h de promedio (altiplano sur, erosión eólica) | SciELO Chile |
| Fotoperiodo / luz | 12 h de luz inducen floración; alta luminosidad favorable | RENAMIPERU (García 1991) |

---

## 3. Suelos del altiplano (dónde se "guarda" el agua)

| Característica | Valor | Fuente |
|---|---|---|
| Textura predominante (intersalar) | **Franco-arenosa y arenosa** | Rev. Boliviana Ciencia (intersalar) |
| Origen | Ceniza volcánica y lava; suelos de laderas más arcillosos y con más MO | LEISA (altiplano sur) |
| Materia orgánica | Muy baja, **~0.7 %** en planicies | LEISA |
| Salinidad | Suelos **altamente salinos** (adaptación de quinua a salinidad) | LEISA; CEDESAN |
| Capacidad de retención de agua | **Baja** en las zonas quinueras (causa de estrés hídrico) | PROINPA |
| Profundidad | Buena en planicies (factor mejor puntuado en calidad de suelo) | Rev. Boliviana Ciencia |
| Pendientes | Laderas más arcillososas, con más nutrientes que las planicies | LEISA |

**Consecuencia para el piloto:** suelos ligeros → poca agua por metro → **TAW bajo** →
umbrales de riego más frecuentes pero con láminas pequeñas; la sonda a 20 cm manda, la de
40 cm vigila percolación (doc 03 §2.2).

---

## 4. Fenología de la quinua

### 4.1 Fases generales y duración (ciclo cultivado en el altiplano)

| Fase | Días post-siembra (DAS) | Notas y riesgos | Fuente |
|---|---|---|---|
| Germinación | **3–5** (emergencia ~8–9) | Depende de humedad y T° suelo; emergencia normal a los 3 d con buenas condiciones | UNSCH; RENAMIPERU |
| Desarrollo vegetativo (2→4→6 hojas) | ~15–37 | Riesgo: humedad del suelo, gusanos cortadores, N | UNSCH; PROINPA |
| Ramificación | ~31–45 | **Fase más tolerante al frío (−8 °C)**; 45–50 ramas/m² en manejo óptimo | UNSCH; CEDESAN; PROINPA |
| Desarrollo del botón floral / panojamiento | ~45–115 | Riesgo: **sequía, heladas**, polilla 2.ª gen. | UNSCH; PROINPA |
| **Floración / antesis** | ~75–115 | **Fase más crítica**: agua y heladas (solo −1 °C) | UNSCH; RENAMIPERU; INIA (Benique & Ojeda 2024) |
| Grano lechoso → pastoso | ~115–140 | Llenado: susceptibilidad a frío | RENAMIPERU |
| **Madurez fisiológica (ciclo total)** | **160–180** (altiplano sur); 150–170 ciclo "normal" FAO/IRD; 154–213 según ecotipo | Riesgo: pájaros, viento, granizada | PROINPA; IRD 2012; SciELO Colombia |

- Ejemplo Cuadro Kankolla (Cabanillas–Puno, RENAMIPERU): ciclo **160 d**, T° prom 10.4 °C
  (máx 16.9 / mín 4.2), precipitación del ciclo 683 mm, **GDD 1 826 (base −1 °C)**.
  *(Zona norte más húmeda que el altiplano boliviano: no trasladar la lluvia, sí los tiempos de fase.)*
- Diversidad: accesiones silvestres de ciclo corto (madurez 95–120 DAS, UNSCH) — el piloto
  usa el **cultivado de altiplano sur, con rango de trabajo 150–180 d**.

### 4.2 GDD (grados-día) para estimar fase

| Parámetro | Valor | Fuente |
|---|---|---|
| Umbral térmico base | **3 °C** (default del piloto) | Jacobsen & Bach 1996 (vía PMC 13646688) |
| Base alternativa | −1 °C (usada en mapa de aptitud Perú) | RENAMIPERU (Cabanillas: 1 826 GDD × ciclo) |
| Temperatura óptima | **15–25 °C** | Gómez y Aguilar 2016 (vía RENAMIPERU) |

---

## 5. Temperatura: límites críticos (alertas)

| Condición | Umbral | Fase más afectada | Fuente |
|---|---|---|---|
| Tolerancia general al frío | hasta **−8 °C** (etapas favorables) | Ramificación (más tolerante) | Mujica et al. 2001 (vía CEDESAN) |
| **Helada en fases sensibles** | **−1 °C** | **Floración y llenado** | Lescano 1994 (vía RENAMIPERU) |
| Daño en formación de grano | hasta −5 °C según variedad | Llenado | Espindola 1986 (vía FAO 2011) |
| Aborto de flores | **> 38 °C** | Floración | Mujica et al. 2001 (vía RENAMIPERU) |
| Temperatura extremo general | −4 a 38 °C; RH 40–88 % | — | FAO 2011 (aq287e) |

**Regla de alerta (doc 06):** helada ≤0 °C = vigilancia; helada ≤ −1 °C **en floración** =
alarma roja; ≥38 °C = alerta de aborto floral.

---

## 6. Agua del cultivo: Kc, ETc y respuesta al déficit

### 6.1 Coeficientes de cultivo (Kc) — set calibrado en el altiplano

| Etapa | **Kc (lisímetro, altiplano boliviano)** | Otros rangos lisimétricos de la literatura | Base |
|---|---|---|---|
| Inicial (siembra–emergencia) | **0.50** | 0.54–0.55 | García et al. 2003 (UMSA/AWM) |
| Media (pleno desarrollo–floración) | **1.00** | 1.16–1.27 | ídem |
| Final (maduración–cosecha) | **0.70** | 0.70–0.90 | ídem |

- **Ky estacional (respuesta a déficit) = 0.67** → la quinua tolera bien el déficit
  (García et al. 2003).
- Nota UMSA: Penman-Monteith **no está calibrado sobre 3 000 m**; por eso este set Kc se
  derivó con lisímetros del altiplano → **usar Kc 0.5/1.0/0.7 y no genéricos de cebada/trigo**.

### 6.2 Consumo total (ETc estacional)

| Dato | Valor | Fuente |
|---|---|---|
| ETc estacional sin estrés (ciclo 150–170 d) | **~500 mm** | IRD 2012 (cap. Quinoa) |
| Rendimiento aceptable con lluvia | **100–200 mm** (quinua muy eficiente en agua) | FAO 2011 (aq287e) |
| ETa reportada en zonas áridas (otros ambientes) | 1 100–1 600 mm | PMC 11351449 (contraste: no aplicar al altiplano) |
| ET mínima para iniciar producción de grano | 210 mm | ScienceDirect 2015 (lisimetría invernadero) |
| ETc diaria ejemplo (Kc 1.0 × ET0 4.8) | ~4.8 mm/día | Cálculo FAO-56 (doc 06) |

### 6.3 Riego deficitario (¿cuánto menos se puede regar?)

| Regla | Valor | Fuente |
|---|---|---|
| Umbral de respuesta útil al riego | **~55 % de ETc** (dejar de abastecer ahí sigue rindiendo) | Geerts et al. 2008 (modelado Bolivia) |
| Déficit sin pérdida significativa | **75 % del riego completo (0.75 FI)**; 0.5 FI sí reduce rendimiento | PMC 9370644 (2022) |
| Calidad del agua | Con agua pobre/salina y napa salina poco profunda, déficit **con precaución** (sur del altiplano) | Taboada et al. 2008b; Winkel et al. 2012 (CONICET) |
| WUE con déficit razonable | ~0.7 kg/m³ (0.75 FI + 250 kg N/ha) | PMC 9370644 |
| Programación práctica (4 riegos por ciclo) | Emergencia, vegetativa, inicio de floración, llenado | PMC 11351449 (costumbre regional) |

---

## 7. Dinámica de agua en el suelo (método FAO-56)

### 7.1 Constantes hídricas

| Constante | Definición | Valor/tensión | Fuente |
|---|---|---|---|
| Saturación | Todos los poros con agua | pF 0 | FAO ai128s |
| **Capacidad de campo (CC)** | Agua tras drenar el exceso | pF ≈ 2 → **−10 a −30 kPa** (1–2 d) | FAO ai128s; METER |
| **Punto de marchitez (PMP)** | Agua no extraíble | pF 4.2 → **−1 500 kPa** | FAO ai128s; METER |
| **TAW** (agua disponible total) | CC − PMP en la zona de raíz | `TAW = 1000 × (θFC − θWP) × Zr` [mm] | FAO-56 ec. 82 |
| **RAW** (agua disponible fácil) | Fracción sin estrés | `RAW = p × TAW` con **p = 0.4–0.7** (default piloto **0.5**) | FAO-56 ec. 83, Tabla 22 |
| Ajuste de p con ETc | Si ETc > 5 mm/d, p baja | `p = p_tabla + 0.04 × (5 − ETc)` | FAO-56 §8.5 (ej. PRISMAB) |
| Umbral general (regla clásica) | Regar cuando se agotó ~**2/3** del agua disponible | MAD ≈ 50–67 % | FAO (y4690s, español) |

**Referencias texturales TAW** (FAO-56, cap. 8/tablas): arena ~40–60 mm/m;
**franco-arenosa ~100 mm/m** (típico del altiplano, §3); limo ~200 mm/m;
arcilla limosa 120 mm/m.

### 7.2 Balance hídrico diario (lo que calcula el agente)

```
Dr(i) = Dr(i−1) − (P − E) − I + ETc        [FAO-56 ec. 85]
ETc = Kc × ET0                             [FAO-56]
si Dr ≥ RAW → regar; lámina = Dr hasta CC − P pronosticada
```

- `Dr` = agotamiento del agua en la raíz [mm]; `P` = lluvia; `E` = evaporación; `I` = riego.
- Los números de `P` y `ET0` los aporta **Agrisignal** (`get_irrigation_advice`,
  `get_growing_conditions`); el suelo, `get_soil_profile`; la fase, `get_growing_degree_days`.

### 7.3 De la teoría al % de la sonda (umbral operativo)

```
ADT (%) = CC − PMP                    (o PMP ≈ CC × 0.55 si no hay análisis)
AFA (%) = p × ADT
Umbral de riego UR (%) = CC − AFA     [PRISMAB / FAO-56]
```

**Ejemplo altiplano** (franco-arenoso: CC 35 %, PMP ≈ 19 %, p = 0.5):

`ADT = 16 %` → `AFA = 8 %` → **UR ≈ 27 %** → la sonda a 20 cm dispara riego bajo 27 %
volumétrico y se reponen ~8 puntos hasta CC.

Otra referencia práctica (−75 kPa como inicio de riego, Cenicaña): humedad de arranque
arena 12 %, arcilla 28 %, franco-arcillosa 33 %; rango óptimo **−20 a −75 kPa**.

### 7.4 Sensores: instalación y lectura (doc 03)

| Práctica | Recomendación | Fuente |
|---|---|---|
| Profundidades | **20 cm** (zona radicular: manda el riego) + **40 cm** (vigila percolación/exceso) | Netafim; Cenicaña |
| Número | ≥2 sondas por unidad de manejo | Redagrícola (Ferreyra) |
| Perfil de raíz quinua | 0.3 m inicial → **0.6–0.8 m** habitual → **hasta 1.8 m** en sequía (suelo liviano) | FAO-56; IRD 2012 |

---

## 8. Parámetros por defecto del piloto (`field_config`)

| Parámetro | Default | Base |
|---|---|---|
| `mad_fraction` (p) | **0.5** (ajustable 0.4–0.6; 0.67 = regla clásica 2/3) | FAO-56 Tabla 22; FAO y4690s |
| `fc_percent` / `pmp_percent` | Medir en sitio; si no, tabla textural §7.1 | FAO-56 |
| Kc por fase | 0.5 / 1.0 / 0.7 | García et al. 2003 |
| `sowing_date` | Real del lote | — |
| GDD base | 3 °C | Jacobsen & Bach 1996 |
| Zr para TAW | 0.3 m → 0.6 m → 0.8 m según fase | FAO-56; IRD 2012 |
| `max_seconds_per_event` | 1 800 s (30 min) | Ajuste hidráulico del usuario |
| `max_events_per_day` | 2 | Práctica regional (4 por ciclo ≈ 150 d) |
| `tank_min_percent` | 20 % | Guarda de operación |

---

## 9. Advertencias de interpretación (el agente lo respeta)

1. **Agrisignal entrega estimaciones** de datos públicos (Open-Meteo/SoilGrids): el agente
   las cita **con números** y no las presenta como medición certificada.
2. **ETo sobre 3 000 m no está calibrado** (UMSA 2003): tratar el ET0 de Agrisignal con
   sensibilidad; la mejora del piloto es calibrarlo con lisímetro/cubeta local.
3. **SoilGrids es 250 m global**: la textura modelada puede diferir del lote → validar con
   calicata una vez y corregir `fc_percent`/`pmp_percent` en `field_config`.
4. **Salinidad**: la quinua germina hasta 52 dS/m con retraso de germinación de hasta
   25 días (CEDESAN); si hay EC en el sensor, considerar estrés osmótico aunque la sonda
   diga "húmedo".
5. **Sequía y riego** no son sinónimos de daño: la quinua rinde con 100–200 mm (FAO 2011);
   el objetivo del piloto es **evitar estrés en floración y maximizar eficiencia**, no
   saturar el suelo.

---

## 10. Síntesis: reglas del agente → fuente

| Regla (doc 06) | Parámetro clave | Fuente § |
|---|---|---|
| R1 Disparo por agotamiento | RAW = p × TAW, p = 0.5 | §7.1 |
| R2 TAW por profundidad de raíz | Zr 0.3–0.8 m (hasta 1.8 m) | §7.4 |
| R3 ETc = Kc × ET0 | Kc 0.5 / 1.0 / 0.7 | §6.1 |
| R4 Crítico en floración (frío −1 °C, >38 °C aborto) | umbrales de alerta | §5 |
| R5 Déficit hasta 75 % FI / umbral 55 % ETc | estrategia con agua escasa | §6.3 |
| R6 Umbral de la sonda | UR = CC − p×(CC−PMP) ≈ 27 % (ejemplo) | §7.3 |
| R7 Guardas: lluvia ≥ lámina, helada, tanque | skip/defer | §5, §6.3 |
| R8 Ciclo 150–180 d, GDD base 3 °C | fase → Kc | §4 |
| R9 Duración/volumen por zona | caudal × tiempo | §8 |

---

## 11. Fuentes (verificadas 2026-10-22)

**FAO (método):**
1. FAO-56 *Evapotranspiración del cultivo* (PDF): <https://www.fao.org/4/x0490s/x0490s.pdf> — cap. 6 Kc: <https://www.fao.org/4/X0490E/x0490e0b.htm> — cap. 8 TAW/RAW/balance: <https://www.fao.org/4/X0490E/x0490e0e.htm>
2. FAO I&D 66 *Crop yield response to water*: <https://www.fao.org/3/i2800e/i2800e.pdf>
3. FAO *Quinoa: an ancient crop to contribute to world food security* (2011): <https://www.fao.org/4/aq287e/aq287e.pdf>
4. FAO *Report on the soils of Bolivia*: <https://www.fao.org/4/15622e/15622e.pdf>
5. FAO *Hidrología, arquitectura del suelo y movimiento del agua* (español): <https://www.fao.org/4/y4690s/y4690s06.htm>
6. FAO *Manual de captación y aprovechamiento del agua de lluvia*: <https://www.fao.org/4/ai128s/ai128s00.pdf>

**Agua y quinua (mediciones):**
7. García, Raes, Jacobsen & Michel (2003). *Evapotranspiration analysis and irrigation requirements of quinoa in the Bolivian highlands* (Kc 0.5/1.0/0.7; Ky 0.67; ETo >3 000 m) — UMSA: <https://pure.umsa.bo/en/publications/evapotranspiration-analysis-and-irrigation-requirements-of-quinoa>
8. IRD (2012) *Quinoa* (cap.: ET ~500 mm/150–170 d; raíz 1.8 m): <https://horizon.documentation.ird.fr/exl-doc/pleins_textes/2021-10/010057413.pdf>
9. Geerts et al. (2008) déficit/agua (umbral 55 % ETc), citado en: <https://www.sciencedirect.com/science/article/abs/pii/S0378377415300196>
10. *Irrigation Regimes and Nitrogen Rates…* (0.75 FI vs 0.5 FI; WUE 0.7 kg/m³): <https://pmc.ncbi.nlm.nih.gov/articles/PMC9370644>
11. *Physiological traits … deficit irrigation and planting methods* (ETa 1 100–1 600 mm; 4 riegos): <https://pmc.ncbi.nlm.nih.gov/articles/PMC11351449>
12. Taboada et al. (2008b) y Winkel et al. (2012) — precaución con déficit/agua salina (comentario): <https://ri.conicet.gov.ar/bitstream/handle/11336/189164/CONICET_Digital_Nro.07d7e374-55ad-438b-9812-27920ae458d3_B.pdf>

**Altiplano y suelos:**
13. *Simulación de la producción de quinua en el Altiplano boliviano (AquaCrop)* — SciELO Bolivia: <http://www.scielo.org.bo/pdf/riiarn/v2n1/v2n1_a03.pdf>
14. PROINPA *Hacia un manejo sustentable de la quinua en el altiplano sur de Bolivia*: <https://www.proinpa.org/web/pdf/Quinua/Manejo%20integrado%20y%20produccion%20de%20la%20quinua/Hacia%20un%20manejo%20sustentable%20de%20la%20quinua%20en%20el%20altiplano%20sur%20de%20Bolivia.pdf>
15. Vacher & Brasier de Thuy *Agricultura y clima sobre el Altiplano boliviano* (IRD): <https://horizon.documentation.ird.fr/exl-doc/pleins_textes/divers11-08/23733.pdf>
16. Winkel (2015) *The Southern Altiplano of Bolivia*: <https://agritrop.cirad.fr/575503/1/document_575503.pdf>
17. *Degradación de suelos… intersalar* (texturas): <http://www.revistasbolivianas.ciencia.bo/pdf/apt/v1n1/v1n1_a08.pdf>
18. *La sustentabilidad del altiplano sur de Bolivia…* — SciELO Chile: <https://www.scielo.cl/scielo.php?pid=S0718-34292017000200002&script=sci_arttext>
19. LEISA *La producción de quinua en el sur de Bolivia*: <https://leisa-al.org/web/revista/volumen-28-numero-04/la-produccion-de-quinua-en-el-sur-de-bolivia-del-exito-economico-al-desastre-ambiental>
20. *Quinoa seed yield and efficiency in soils deficient of N in the Bolivian Altiplano* (237 mm SENAMHI): <https://pmc.ncbi.nlm.nih.gov/articles/PMC8624588>

**Fenología y temperatura:**
21. *Fenología de tres accesiones de quinua silvestre* (UNSCH): <https://revistas.unsch.edu.pe/index.php/investigacion/article/download/367/304/611>
22. *Descripción del ciclo fenológico de cuatro ecotipos de quinua* (Puracé) — SciELO Colombia: <http://www.scielo.org.co/scielo.php?pid=S1692-35612018000200026&script=sci_arttext>
23. CEDESAN *Investigación sobre el cultivo de la quinua* (tolerancias, salinidad, siembra): <https://portal.siinsan.gob.gt/wp-content/uploads/cedesan2/libros/INVESTIGACION-SOBRE-EL-CULTIVO-DE-QUINUA-V2.pdf>
24. RENAMIPERU/MINAGRI *Mapa de aptitud climática de la quinua* (GDD, −1 °C, −8/−1/38 °C): <https://cdn.www.gob.pe/uploads/document/file/2253265/QUINUA.pdf>
25. Jacobsen & Bach 1996 (GDD base 3 °C) vía *Phenotypic differentiation … quinoa*: <https://pmc.ncbi.nlm.nih.gov/articles/PMC13646688>
26. INIA Perú — huella hídrica de quinua (floración = fase más crítica): <https://repositorio.inia.gob.pe/server/api/core/bitstreams/e43c29c3-4765-4e70-b63d-55338fd1893d/content>
27. *State of the art report on quinoa around the world* (2013): <https://agritrop.cirad.fr/575489/1/document_575489.pdf>

**Sensores y umbrales operativos:**
28. PRISMAB *Cuándo regar con sensores de humedad de suelo* (UR = CC − AFA; PMP ≈ 0.55×CC): <https://prismab.com/blog/cuando-regar-con-sensores-de-humedad-de-suelo>
29. Cenicaña *ABC de los sensores de humedad* (−75 kPa; 12/28/33 %; rango −20 a −75 kPa): <https://www.cenicana.org/abc-de-los-sensores-de-humedad-en-las-labores-de-riego>
30. Netafim *Sensores de humedad* (20 + 40 cm): <https://www.netafim.com/es-co/blog/importancia-de-los-sensores-de-humedad-para-disenar-una-estrategia-de-riego-exitosa>
31. METER Group *Curvas de liberación de humedad* (CC −10…−30 kPa; PMP −1 500 kPa): <https://metergroup.com/es/measurement-insights/soil-moisture-release-curves-what-they-are-why-you-need-them-how-to-use-them/>
32. Extensión UMN *Conceptos básicos de la programación del riego*: <https://es.extension.umn.edu/riego/programaci%C3%B3n-b%C3%A1sica-del-riego>
33. Redagrícola *Programa de riego en palto* (≥2 sondas/unidad de manejo): <https://redagricola.com/recomendaciones-para-un-optimo-programa-de-riego-en-palto>

**Clima (SENAMHI):**
34. SENAMHI Bolivia — monitoreo climático: <https://senamhi.gob.bo/index.php/clima/monitoreo-climatico>
35. Alerta de heladas altiplano (jul 2026, mín. < 0 °C, máx 16–21 °C): <https://www.reduno.com.bo/noticias/alertan-heladas-senamhi-preve-que-bajas-temperaturas-continuen-la-siguiente-semana-202672419173>
