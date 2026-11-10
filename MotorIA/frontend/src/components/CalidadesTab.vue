<script setup>
import { ref, reactive, computed, onMounted } from "vue";
import { api, apiUrl } from "../api.js";
import { useTipos } from "../composables/useTipos.js";
import { fechaCorta } from "../utils/format.js";
import AppIcon from "./AppIcon.vue";
import QualityChart from "./QualityChart.vue";
import PdfPreviewModal from "./PdfPreviewModal.vue";

const { activos, loadTipos, claseDe } = useTipos();

const INTENCIONES = {
  general: "Consultas generales",
  definicion: "Definiciones",
  procedimiento: "Procedimientos",
  autoridad: "Autoridades",
  requisito: "Requisitos",
  plazo: "Plazos y vigencia",
};

const PRUEBAS = [
  {
    nombre: "Primer resultado",
    mide: "De cada 100 consultas de prueba, en cuántas el sistema colocó la respuesta correcta como la coincidencia N.º 1.",
    como: "80 % o más es excelente: el usuario encuentra la información de inmediato.",
  },
  {
    nombre: "Dentro de los 3 mejores",
    mide: "En cuántas consultas la respuesta correcta quedó entre las 3 mejores coincidencias.",
    como: "Si no aparece en primer lugar, al menos queda en la primera pantalla de resultados.",
  },
  {
    nombre: "Dentro de los 5 mejores",
    mide: "En cuántas consultas la respuesta correcta quedó entre las 5 mejores (cobertura general del ranking).",
    como: "Indica que la información existe en la base y el buscador la recupera.",
  },
  {
    nombre: "Similitud del contenido",
    mide: "Qué tan parecidos son los fragmentos encontrados a la pregunta formulada.",
    como: "Valor de 0 (nada parecido) a 1 (muy parecido en significado).",
  },
];

const docs = ref([]);
const resultados = reactive({});
const verificando = reactive({});
const cargando = ref(false);
const error = ref("");

const pdfPreview = ref(null);

const general = ref(null);
const generalCargando = ref(false);
const generalError = ref("");
const numPreguntas = ref(20);
const probarRespuestas = ref(false);
const numRespuestas = ref(2);
const muestraMetodo = ref(false);
const muestraLlm = ref(false);
const muestraQueries = ref(false);

const verificandoTodos = ref(false);
const progresoTodos = ref(0);

const docsVerificados = computed(() => Object.keys(resultados).length);

function calidadDe(doc) {
  const r = resultados[doc.id];
  if (!r || !r.total_queries) return null;
  return Math.round((r.precision_at_5 ?? 0) * 100);
}

function colorDe(val) {
  if (val == null) return "";
  if (val >= 80) return "high";
  if (val >= 50) return "mid";
  return "low";
}

async function loadDocumentos() {
  cargando.value = true;
  error.value = "";
  try {
    const r = await api.documentos();
    const d = await r.json();
    docs.value = d.documentos || [];
  } catch {
    error.value = "Error al cargar los documentos";
  } finally {
    cargando.value = false;
  }
}

async function verificarUno(id) {
  verificando[id] = true;
  try {
    const r = await api.verificar(id);
    resultados[id] = await r.json();
  } catch {
    resultados[id] = { total_queries: 0, precision_at_1: 0, precision_at_3: 0, precision_at_5: 0, average_similarity: 0 };
  } finally {
    verificando[id] = false;
  }
}

async function verificarTodos() {
  verificandoTodos.value = true;
  progresoTodos.value = 0;
  for (const doc of docs.value) {
    if (!resultados[doc.id]) {
      await verificarUno(doc.id);
    }
    progresoTodos.value += 1;
  }
  verificandoTodos.value = false;
}

function verPDF(doc) {
  pdfPreview.value = doc;
}

async function ejecutarGeneral() {
  generalCargando.value = true;
  generalError.value = "";
  general.value = null;
  try {
    const r = await api.verificarSistema({
      num_preguntas: numPreguntas.value,
      probar_respuestas: probarRespuestas.value,
      num_respuestas: numRespuestas.value,
    });
    general.value = await r.json();
  } catch {
    generalError.value = "Error al ejecutar la evaluación. Intente nuevamente.";
  } finally {
    generalCargando.value = false;
  }
}

function intencionLabel(k) {
  return INTENCIONES[k] || k;
}

function pct(v) {
  return Math.round((v ?? 0) * 100) + "%";
}

onMounted(async () => {
  await loadTipos();
  await loadDocumentos();
});
</script>

<template>
  <div class="tab-pane qa-pane">
    <!-- ============ Sección 1: Evaluación general del sistema ============ -->
    <section class="qa-section">
      <header class="tab-header">
        <div>
          <h3>Evaluación del sistema</h3>
          <div class="sub">
            Prueba toda la plataforma de una sola vez: búsqueda híbrida, ranking y respuestas del modelo.
          </div>
        </div>
        <div class="header-actions">
          <label class="qa-num">
            Consultas
            <select v-model.number="numPreguntas" class="form-control filter-select" :disabled="generalCargando">
              <option :value="10">10</option>
              <option :value="20">20</option>
              <option :value="30">30</option>
            </select>
          </label>
          <button class="btn btn--primary" @click="ejecutarGeneral" :disabled="generalCargando">
            <span v-if="generalCargando" class="spin"></span>
            <AppIcon v-else name="zap" :size="16" />
            {{ generalCargando ? "Evaluando…" : general ? "Reevaluar" : "Evaluar sistema" }}
          </button>
        </div>
      </header>

      <div v-if="generalCargando" class="card qa-loading">
        <div class="spinner"></div>
        <p>Ejecutando {{ numPreguntas }} consultas de prueba sobre toda la base de conocimiento…</p>
        <p class="qa-loading-sub">Puede tomar alrededor de un minuto.</p>
      </div>

      <div v-else-if="generalError" class="card result result--error">{{ generalError }}</div>

      <div v-else-if="general && !general.error" class="qa-results">
        <div class="qa-summary">
          <div class="qa-chart">
            <QualityChart
              :verificacion="general"
              :indice="general.indice_general"
              titulo="Índice general del sistema"
            />
          </div>

          <div class="qa-method">
            <h4>Cómo se midió</h4>
            <ol>
              <li>Se tomaron <strong>{{ general.total_queries }} oraciones reales</strong> al azar de los {{ general.total_documentos }} documentos indexados.</li>
              <li>Cada oración se usó como consulta contra el <strong>mismo buscador</strong> que usa el chat (semántica + texto + detección de intención).</li>
              <li>Se verificó en qué posición aparece el fragmento correcto de cada consulta.</li>
            </ol>
            <button
              class="method-toggle"
              :class="{ open: muestraMetodo }"
              @click="muestraMetodo = !muestraMetodo"
            >
              <span class="ft-left">
                <span class="app-icon"><AppIcon name="info" :size="14" /></span>
                ¿Qué significa cada prueba?
              </span>
              <span class="chevron" :class="{ open: muestraMetodo }">
                <AppIcon name="chevron-down" :size="13" />
              </span>
            </button>
            <Transition name="fuentes">
              <div v-if="muestraMetodo" class="pruebas-table">
                <div v-for="p in PRUEBAS" :key="p.nombre" class="prueba-item">
                  <strong>{{ p.nombre }}</strong>
                  <span>{{ p.mide }}</span>
                  <em>{{ p.como }}</em>
                </div>
              </div>
            </Transition>
          </div>
        </div>

        <div class="qa-breakdown">
          <div class="card bd-card">
            <h4>Desglose por tipo de consulta</h4>
            <div v-if="!general.por_intencion.length" class="empty-state">Sin datos.</div>
            <div v-else class="bd-rows">
              <div v-for="g in general.por_intencion" :key="g.intencion" class="bd-row">
                <span class="bd-label">{{ intencionLabel(g.intencion) }}</span>
                <div class="bd-track">
                  <div class="bd-fill" :class="colorDe(g.precision_at_5 * 100)" :style="{ width: pct(g.precision_at_5) }"></div>
                </div>
                <span class="bd-value" :class="colorDe(g.precision_at_5 * 100)">{{ pct(g.precision_at_5) }}</span>
                <span class="bd-total">{{ g.total }} consultas</span>
              </div>
            </div>
          </div>

          <div class="card bd-card">
            <h4>Desglose por tipo de norma</h4>
            <div v-if="!general.por_tipo.length" class="empty-state">Sin datos.</div>
            <div v-else class="bd-rows">
              <div v-for="g in general.por_tipo" :key="g.tipo_id" class="bd-row">
                <span class="bd-label">{{ g.tipo_nombre }}</span>
                <div class="bd-track">
                  <div class="bd-fill" :class="colorDe(g.precision_at_5 * 100)" :style="{ width: pct(g.precision_at_5) }"></div>
                </div>
                <span class="bd-value" :class="colorDe(g.precision_at_5 * 100)">{{ pct(g.precision_at_5) }}</span>
                <span class="bd-total">{{ g.total }} consultas</span>
              </div>
            </div>
          </div>
        </div>

        <div v-if="probarRespuestas && general.llm && general.llm.total_respuestas" class="card qa-llm">
          <h4>Prueba de respuestas del modelo</h4>
          <p class="qa-llm-sub">
            Se generaron respuestas reales con el modelo de lenguaje y se verificó que citen sus fuentes.
          </p>
          <div class="llm-cards">
            <div class="llm-stat">
              <span class="llm-num">{{ pct(general.llm.tasa_citacion) }}</span>
              <span class="llm-tag">Respuestas con cita</span>
            </div>
            <div class="llm-stat">
              <span class="llm-num">{{ Math.round(general.llm.tiempo_promedio_seg * 10) / 10 }} s</span>
              <span class="llm-tag">Tiempo promedio</span>
            </div>
            <div class="llm-stat">
              <span class="llm-num">{{ general.llm.tokens_promedio }}</span>
              <span class="llm-tag">Tokens promedio</span>
            </div>
          </div>
          <button
            class="method-toggle"
            :class="{ open: muestraLlm }"
            @click="muestraLlm = !muestraLlm"
          >
            <span class="ft-left">
              <span class="app-icon"><AppIcon name="eye" :size="14" /></span>
              Ver las {{ general.llm.total_respuestas }} respuestas generadas
            </span>
            <span class="chevron" :class="{ open: muestraLlm }">
              <AppIcon name="chevron-down" :size="13" />
            </span>
          </button>
          <Transition name="fuentes">
            <div v-if="muestraLlm" class="llm-list">
              <div v-for="(r, i) in general.llm.resultados" :key="i" class="llm-item">
                <div class="llm-head">
                  <span class="llm-q">P{{ i + 1 }}. {{ r.query }}</span>
                  <span class="badge" :class="r.cito_fuentes ? 'badge--ok' : 'badge--no'">
                    {{ r.cito_fuentes ? "Citó fuentes" : "Sin cita" }}
                  </span>
                  <span class="llm-time">{{ r.tiempo_seg }} s · {{ r.tokens }} tokens</span>
                </div>
                <p class="llm-r">{{ r.respuesta }}</p>
              </div>
            </div>
          </Transition>
        </div>

        <div class="card qa-queries">
          <button
            class="method-toggle"
            :class="{ open: muestraQueries }"
            @click="muestraQueries = !muestraQueries"
          >
            <span class="ft-left">
              <span class="app-icon"><AppIcon name="list" :size="14" /></span>
              Ver las {{ general.total_queries }} consultas de prueba y su resultado
            </span>
            <span class="chevron" :class="{ open: muestraQueries }">
              <AppIcon name="chevron-down" :size="13" />
            </span>
          </button>
          <Transition name="fuentes">
            <div v-if="muestraQueries" class="queries-list">
              <div v-for="(q, i) in general.queries" :key="i" class="qa-item">
                <div class="qa-item-q">
                  <span class="qa-tag qa-tag--q">Pregunta {{ i + 1 }}</span>
                  <p class="qa-pregunta">{{ q.query }}</p>
                </div>
                <div class="qa-item-r">
                  <span class="qa-tag qa-tag--r">Resultado</span>
                  <div class="qa-resultado">
                    <span class="qa-pos" :class="q.passed ? 'ok' : 'no'">
                      <AppIcon :name="q.passed ? 'check' : 'x'" :size="12" />
                      {{ q.passed ? `Encontrada en la posición #${q.found_position}` : "No se encontró el fragmento" }}
                    </span>
                    <span class="qa-sim" :class="colorDe(q.similitud * 100)">
                      Similitud: {{ pct(q.similitud) }}
                    </span>
                  </div>
                  <p v-if="q.respuesta" class="qa-respuesta">“{{ q.respuesta }}”</p>
                  <p v-if="q.documento" class="qa-meta-doc">
                    {{ q.documento }} · {{ intencionLabel(q.intencion) }}
                  </p>
                </div>
              </div>
            </div>
          </Transition>
        </div>
      </div>

      <div v-else class="card qa-empty">
        <div class="qa-empty-icon"><AppIcon name="bar-chart" :size="26" /></div>
        <p><strong>Ejecute una evaluación general</strong></p>
        <p>
          El sistema tomará consultas al azar de todos los documentos, las buscará como lo hace el chat
          y medirá qué tan bien encuentra la información.
        </p>
        <label class="qa-check">
          <input v-model="probarRespuestas" type="checkbox" />
          También probar las respuestas del modelo de lenguaje
          <span class="qa-warn">(tarda varios minutos en este equipo)</span>
        </label>
      </div>
    </section>

    <!-- ============ Sección 2: Calidad por documento ============ -->
    <section class="qa-section">
      <header class="tab-header">
        <div>
          <h3>Calidad por documento</h3>
          <div class="sub">
            {{ docs.length }} documento(s) · {{ docsVerificados }} verificados
            <template v-if="progresoTodos"> · progreso {{ progresoTodos }}/{{ docs.length }}</template>
          </div>
        </div>
        <div class="header-actions">
          <button class="btn btn--ghost" @click="loadDocumentos" :disabled="verificandoTodos" title="Actualizar">
            <AppIcon name="refresh" :size="16" />
            Actualizar
          </button>
          <button class="btn btn--primary" @click="verificarTodos" :disabled="verificandoTodos || !docs.length">
            <span v-if="verificandoTodos" class="spin"></span>
            <AppIcon v-else name="zap" :size="16" />
            {{ verificandoTodos ? "Verificando…" : "Verificar todos" }}
          </button>
        </div>
      </header>

      <p v-if="error" class="result result--error">{{ error }}</p>

      <div v-if="cargando" class="qa-grid">
        <div v-for="i in 4" :key="i" class="card qa-doc-card">
          <div class="skeleton qa-card-skel"></div>
        </div>
      </div>

      <div v-else-if="!docs.length" class="empty-state">
        No hay documentos cargados. Use la pestaña Biblioteca para subir normas.
      </div>

      <div v-else class="qa-grid">
        <div v-for="doc in docs" :key="doc.id" class="card qa-doc-card">
          <div class="qa-doc-head">
            <div class="qa-doc-meta">
              <span class="badge" :class="claseDe(doc.prioridad ?? doc.tipo_id)">{{ doc.tipo_nombre }}</span>
              <span class="qa-doc-secciones">{{ doc.total_chunks }} secciones</span>
              <span v-if="doc.fecha_publicacion" class="qa-doc-fecha">Pub. {{ fechaCorta(doc.fecha_publicacion) }}</span>
            </div>
            <h4 class="qa-doc-titulo">{{ doc.titulo }}</h4>

            <div class="qa-doc-actions">
              <span
                v-if="verificando[doc.id]"
                class="quality-badge evaluating"
              ><span class="spin-dark"></span> Evaluando…</span>
              <span
                v-else-if="calidadDe(doc) != null"
                class="quality-badge"
                :class="colorDe(calidadDe(doc))"
              >{{ calidadDe(doc) }}%</span>
              <span v-else class="quality-badge none">Sin evaluar</span>

              <button class="btn btn--ghost btn--sm" @click="verPDF(doc)" title="Ver el documento PDF">
                <AppIcon name="file" :size="14" />
                Ver PDF
              </button>
              <button class="btn btn--primary btn--sm" @click="verificarUno(doc.id)" :disabled="verificando[doc.id]">
                <span v-if="verificando[doc.id]" class="spin"></span>
                <AppIcon v-else name="zap" :size="14" />
                {{ resultados[doc.id] && resultados[doc.id].total_queries ? "Volver a verificar" : "Verificar este documento" }}
              </button>
            </div>
          </div>

          <div class="qa-doc-body">
            <div v-if="verificando[doc.id]" class="detail-loading">
              <span class="spin-dark"></span> Verificando este documento…
            </div>
            <template v-else-if="resultados[doc.id] && resultados[doc.id].total_queries">
              <QualityChart
                :verificacion="resultados[doc.id]"
                :titulo="`Calidad de “${doc.titulo}”`"
              />

              <div
                v-if="resultados[doc.id].queries && resultados[doc.id].queries.length"
                class="queries-list"
              >
                <p class="queries-title">Preguntas de prueba ({{ resultados[doc.id].queries.length }})</p>
                <div v-for="(q, i) in resultados[doc.id].queries" :key="i" class="qa-item">
                  <div class="qa-item-q">
                    <span class="qa-tag qa-tag--q">Pregunta {{ i + 1 }}</span>
                    <p class="qa-pregunta">{{ q.query }}</p>
                  </div>
                  <div class="qa-item-r">
                    <span class="qa-tag qa-tag--r">Resultado</span>
                    <div class="qa-resultado">
                      <span class="qa-pos" :class="q.passed ? 'ok' : 'no'">
                        <AppIcon :name="q.passed ? 'check' : 'x'" :size="12" />
                        {{ q.passed ? `Encontrada en la posición #${q.found_position}` : "No se encontró el fragmento" }}
                      </span>
                      <span class="qa-sim" :class="colorDe(q.similitud * 100)">
                        Similitud: {{ pct(q.similitud) }}
                      </span>
                    </div>
                    <p v-if="q.respuesta" class="qa-respuesta">“{{ q.respuesta }}”</p>
                  </div>
                </div>
              </div>
            </template>
            <p v-else class="empty-state qa-doc-hint">
              Pulse <strong>“Verificar este documento”</strong> para medir su calidad de indexación.
            </p>
          </div>
        </div>
      </div>
    </section>
  </div>

  <PdfPreviewModal
    :show="Boolean(pdfPreview)"
    :doc="pdfPreview"
    :url="pdfPreview ? apiUrl(`/documentos/${pdfPreview.id}/descargar?inline=1`) : ''"
    @close="pdfPreview = null"
  />
</template>

<style scoped>
.qa-pane {
  padding-bottom: 60px;
}

.qa-section {
  margin-bottom: 34px;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.qa-num {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text-muted);
}

.filter-select {
  width: auto;
  min-width: 70px;
  padding: 7px 28px 7px 10px;
  font-size: 13px;
}

.qa-empty,
.qa-loading {
  text-align: center;
  padding: 40px 24px;
  color: var(--text-muted);
  font-size: 14px;
}

.qa-empty-icon {
  width: 56px;
  height: 56px;
  margin: 0 auto 14px;
  border-radius: 16px;
  display: grid;
  place-items: center;
  background: var(--accent-soft);
  color: var(--accent);
}

.qa-empty p strong {
  display: block;
  color: var(--text);
  margin-bottom: 4px;
}

.qa-check {
  display: inline-flex;
  align-items: flex-start;
  gap: 8px;
  margin-top: 18px;
  font-size: 13px;
  color: var(--text-muted);
  text-align: left;
}

.qa-check input {
  margin-top: 3px;
  accent-color: var(--accent);
}

.qa-warn {
  color: var(--warning);
  font-weight: 600;
}

.qa-loading .spinner {
  width: 34px;
  height: 34px;
  border: 3px solid var(--accent-soft);
  border-top-color: var(--accent);
  border-radius: 50%;
  margin: 0 auto 14px;
  animation: rot 0.8s linear infinite;
}

.qa-loading-sub {
  font-size: 12.5px;
  color: var(--text-dim);
  margin-top: 4px;
}

.qa-summary {
  display: grid;
  grid-template-columns: 1.1fr 1fr;
  gap: 16px;
  align-items: stretch;
}

.qa-chart {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 20px;
  box-shadow: var(--shadow);
}

.qa-method {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 20px;
  box-shadow: var(--shadow);
}

.qa-method h4,
.bd-card h4,
.qa-llm h4 {
  font-size: 13px;
  font-weight: 700;
  color: var(--text);
  margin-bottom: 12px;
}

.qa-method ol {
  padding-left: 18px;
  font-size: 13px;
  color: var(--text-muted);
  line-height: 1.6;
}

.qa-method li {
  margin-bottom: 6px;
}

.method-toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  margin-top: 14px;
  padding: 9px 12px;
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text-muted);
  background: var(--bg-soft);
  border: 1px solid var(--border-soft);
  border-radius: 8px;
}

.method-toggle:hover {
  color: var(--text);
  border-color: var(--border);
}

.ft-left {
  display: inline-flex;
  align-items: center;
  gap: 8px;
}

.chevron {
  display: grid;
  place-items: center;
  transition: transform 0.2s ease;
}

.chevron.open {
  transform: rotate(180deg);
}

.pruebas-table {
  display: flex;
  flex-direction: column;
  gap: 10px;
  margin-top: 10px;
}

.prueba-item {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 10px 12px;
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: 10px;
}

.prueba-item strong {
  font-size: 12.5px;
  color: var(--text);
}

.prueba-item span {
  font-size: 12px;
  color: var(--text-muted);
  line-height: 1.5;
}

.prueba-item em {
  font-size: 11.5px;
  color: var(--text-dim);
  font-style: normal;
}

.qa-breakdown {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  margin-top: 16px;
}

.bd-rows {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.bd-row {
  display: flex;
  align-items: center;
  gap: 10px;
}

.bd-label {
  flex: 0 0 34%;
  font-size: 12.5px;
  color: var(--text-muted);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.bd-track {
  flex: 1;
  height: 8px;
  border-radius: 999px;
  background: var(--bg-soft);
  overflow: hidden;
}

.bd-fill {
  height: 100%;
  border-radius: 999px;
  transition: width 0.7s ease;
}

.bd-fill.high { background: var(--success); }
.bd-fill.mid { background: var(--warning); }
.bd-fill.low { background: var(--danger); }

.bd-value {
  flex: 0 0 44px;
  text-align: right;
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
}

.bd-value.high { color: var(--success); }
.bd-value.mid { color: var(--warning); }
.bd-value.low { color: var(--danger); }

.bd-total {
  flex: 0 0 auto;
  font-size: 10.5px;
  color: var(--text-dim);
  background: var(--surface-3);
  border-radius: 6px;
  padding: 2px 7px;
}

.qa-llm {
  margin-top: 16px;
}

.qa-llm-sub {
  font-size: 12.5px;
  color: var(--text-muted);
  margin-bottom: 14px;
}

.llm-cards {
  display: flex;
  gap: 12px;
  margin-bottom: 12px;
  flex-wrap: wrap;
}

.llm-stat {
  flex: 1;
  min-width: 130px;
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: 12px;
  padding: 12px 14px;
}

.llm-num {
  display: block;
  font-family: var(--mono);
  font-size: 20px;
  font-weight: 700;
  color: var(--accent-strong);
}

.llm-tag {
  display: block;
  font-size: 11px;
  color: var(--text-muted);
  margin-top: 2px;
}

.llm-list,
.queries-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
}

.llm-item {
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: 10px;
  padding: 10px 12px;
}

.llm-head {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.llm-q {
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text);
}

.llm-time {
  margin-left: auto;
  font-size: 11px;
  color: var(--text-dim);
}

.llm-r {
  margin-top: 6px;
  font-size: 12.5px;
  color: var(--text-muted);
  line-height: 1.55;
}

.qa-queries {
  margin-top: 16px;
}

.qa-queries .method-toggle {
  margin-top: 0;
}

.qa-item {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 12px 14px;
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: 10px;
}

.qa-item-q {
  display: flex;
  gap: 10px;
  align-items: flex-start;
}

.qa-item-r {
  display: flex;
  gap: 10px;
  align-items: flex-start;
  padding-top: 8px;
  border-top: 1px dashed var(--border-soft);
}

.qa-tag {
  flex-shrink: 0;
  font-size: 10.5px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  border-radius: 6px;
  padding: 3px 8px;
  margin-top: 1px;
}

.qa-tag--q {
  background: var(--accent-soft);
  color: var(--accent-strong);
}

.qa-tag--r {
  background: var(--surface-3);
  color: var(--text-dim);
}

.qa-pregunta {
  font-size: 13px;
  font-weight: 600;
  color: var(--text);
  line-height: 1.5;
  white-space: pre-wrap;
}

.qa-resultado {
  flex: 1;
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.qa-pos {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-size: 12px;
  font-weight: 600;
}

.qa-pos.ok {
  color: var(--success);
}

.qa-pos.no {
  color: var(--danger);
}

.qa-sim {
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
}

.qa-sim.high { color: var(--success); }
.qa-sim.mid { color: var(--warning); }
.qa-sim.low { color: var(--danger); }

.qa-respuesta {
  flex: 1;
  font-size: 12px;
  color: var(--text-muted);
  line-height: 1.55;
  white-space: pre-wrap;
}

.qa-meta-doc {
  width: 100%;
  font-size: 11px;
  color: var(--text-dim);
  margin-top: 2px;
}

.qa-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(420px, 1fr));
  gap: 16px;
}

.qa-doc-card {
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.qa-card-skel {
  height: 220px;
  margin: 0;
}

.qa-doc-head {
  padding: 16px 18px;
  border-bottom: 1px solid var(--border-soft);
}

.qa-doc-meta {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 8px;
}

.qa-doc-secciones,
.qa-doc-fecha {
  font-size: 11px;
  color: var(--text-dim);
  background: var(--surface-3);
  border-radius: 6px;
  padding: 2px 7px;
}

.qa-doc-titulo {
  font-size: 14.5px;
  font-weight: 700;
  color: var(--text);
  line-height: 1.35;
  margin-bottom: 12px;
}

.qa-doc-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.qa-doc-actions .btn {
  margin-left: 0;
}

.qa-doc-actions .btn:last-child {
  margin-left: auto;
}

.qa-doc-body {
  padding: 16px 18px 18px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.qa-doc-hint {
  margin: 6px 0;
}

.qa-doc-body .queries-list {
  margin-top: 4px;
}

.detail-loading {
  display: flex;
  align-items: center;
  gap: 10px;
  color: var(--text-muted);
  font-size: 13px;
  padding: 8px 0;
}

.spin-dark {
  width: 14px;
  height: 14px;
  border: 2px solid var(--accent-soft);
  border-top-color: var(--accent);
  border-radius: 50%;
  display: inline-block;
  animation: rot 0.7s linear infinite;
}

.quality-badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  min-width: 74px;
  justify-content: center;
  padding: 4px 12px;
  border-radius: 999px;
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
}

.quality-badge.high {
  background: var(--success-soft);
  color: var(--success);
}

.quality-badge.mid {
  background: rgba(217, 119, 6, 0.12);
  color: var(--warning);
}

.quality-badge.low {
  background: var(--danger-soft);
  color: var(--danger);
}

.quality-badge.none {
  background: var(--surface-3);
  color: var(--text-dim);
}

.quality-badge.evaluating {
  background: var(--accent-soft);
  color: var(--accent-strong);
  font-family: var(--font);
  font-weight: 600;
}

.queries-title {
  margin-top: 18px;
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text-muted);
}

@media (max-width: 900px) {
  .qa-summary,
  .qa-breakdown {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 700px) {
  .qa-grid {
    grid-template-columns: 1fr;
  }

  .qa-doc-actions .btn:last-child {
    margin-left: 0;
  }
}
</style>
