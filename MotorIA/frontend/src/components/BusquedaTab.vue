<script setup>
import { ref, onMounted } from "vue";
import { api } from "../api.js";
import { useTipos } from "../composables/useTipos.js";
import { escapeHtml, pct } from "../utils/format.js";
import AppIcon from "./AppIcon.vue";

const { activos, loadTipos, claseDe } = useTipos();

const pregunta = ref("");
const tipoId = ref("");
const limite = ref(10);
const umbral = ref(30);
const buscando = ref(false);
const resultados = ref([]);
const total = ref(0);
const error = ref("");
const busco = ref(false);

async function buscar() {
  const q = pregunta.value.trim();
  if (!q || buscando.value) return;

  buscando.value = true;
  error.value = "";
  resultados.value = [];
  busco.value = false;

  try {
    const body = {
      pregunta: q,
      limite: Math.max(1, Math.min(20, parseInt(limite.value) || 10)),
      umbral: (parseInt(umbral.value) || 30) / 100,
    };
    if (tipoId.value) body.tipo_id = parseInt(tipoId.value);

    const r = await api.busqueda(body);
    const ct = r.headers.get("content-type") || "";
    if (!ct.includes("application/json")) {
      throw new Error("El servidor devolvió una respuesta inesperada, intente nuevamente.");
    }
    const d = await r.json();

    if (d.error) {
      error.value = d.error;
    } else {
      resultados.value = d.resultados || [];
      total.value = d.total_resultados || resultados.value.length;
      busco.value = true;
    }
  } catch (err) {
    error.value = "Error de conexión: " + err.message;
  } finally {
    buscando.value = false;
  }
}

function onKeydown(e) {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    buscar();
  }
}

onMounted(loadTipos);
</script>

<template>
  <div class="tab-pane tab-pane--center">
    <div class="busqueda-wrap">
      <header class="busq-header">
        <h3>Búsqueda semántica</h3>
        <p class="sub">
          Encuentra fragmentos relevantes en la base de conocimiento sin usar el modelo de lenguaje. Ideal para localizar
          artículos y numerales específicos.
        </p>
      </header>

      <div class="card busq-form">
        <div class="form-group">
          <label for="busquedaQ">Consulta</label>
          <textarea id="busquedaQ" v-model="pregunta" rows="2" class="form-control"
            placeholder="Ej: requisitos para una ordenanza municipal de áreas verdes" @keydown="onKeydown"></textarea>
        </div>

        <div class="busq-options">
          <div class="form-group">
            <label for="busquedaTipo">Tipo de norma</label>
            <select id="busquedaTipo" v-model="tipoId" class="form-control">
              <option value="">Todos</option>
              <option v-for="t in activos" :key="t.id" :value="t.id">{{ t.prioridad }} · {{ t.nombre }}</option>
            </select>
          </div>
          <div class="form-group">
            <label for="busquedaLimite">Resultados (1-20)</label>
            <input id="busquedaLimite" v-model.number="limite" type="number" min="1" max="20" class="form-control" />
          </div>
          <div class="form-group">
            <label>Similitud mínima: <strong class="accent">{{ umbral }}%</strong></label>
            <input v-model.number="umbral" type="range" min="0" max="100" />
          </div>
        </div>

        <button class="btn btn--primary btn--block" @click="buscar" :disabled="buscando || !pregunta.trim()">
          <span v-if="buscando" class="spin"></span>
          <AppIcon v-else name="search" :size="16" />
          {{ buscando ? "Buscando…" : "Buscar" }}
        </button>
      </div>

      <p v-if="error" class="result result--error">{{ error }}</p>

      <div v-if="buscando" class="result-list">
        <div v-for="i in 3" :key="i" class="skeleton res-skel"></div>
      </div>

      <template v-else-if="busco">
        <p v-if="!resultados.length" class="empty-state">
          No se encontraron resultados con la similitud mínima indicada.
        </p>
        <template v-else>
          <p class="results-count">{{ total }} resultado(s) para "{{ pregunta.trim() }}"</p>
          <div class="result-list">
            <div v-for="(res, i) in resultados" :key="i" class="card result-card">
              <div class="result-head">
                <span class="badge" :class="claseDe(res.prioridad ?? res.tipo_id)">{{ res.tipo }}</span>
                <strong class="res-titulo">{{ res.titulo }}</strong>
                <span v-if="res.numeral" class="fuente-numeral">({{ res.numeral }})</span>
                <span class="res-similitud">{{ pct(res.similitud) }}</span>
              </div>
              <div class="similitud-track">
                <div class="similitud-fill" :style="{ width: (res.similitud * 100).toFixed(1) + '%' }"></div>
              </div>
              <div class="result-content">{{ res.contenido }}</div>
            </div>
          </div>
        </template>
      </template>
    </div>
  </div>
</template>

<style scoped>
.busqueda-wrap {
  width: min(760px, 100%);
  margin: 16px 0;
}

.busq-header {
  margin-bottom: 18px;
}

.busq-header h3 {
  font-size: 20px;
  font-weight: 700;
}

.busq-header .sub {
  color: var(--text-muted);
  font-size: 13px;
  margin-top: 4px;
}

.busq-form {
  margin-bottom: 20px;
}

.busq-options {
  display: grid;
  grid-template-columns: 1.4fr 0.8fr 1fr;
  gap: 14px;
}

.accent {
  color: var(--accent);
}

.result-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
  margin-top: 12px;
}

.res-skel {
  height: 120px;
}

.results-count {
  font-size: 13px;
  color: var(--text-muted);
  margin-top: 16px;
}

.result-card {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.result-head {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
}

.res-titulo {
  font-size: 14px;
}

.fuente-numeral {
  color: var(--text-dim);
  font-size: 12px;
}

.res-similitud {
  margin-left: auto;
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
  color: var(--accent);
}

.similitud-track {
  height: 5px;
  border-radius: 999px;
  background: var(--bg-soft);
  border: 1px solid var(--border-soft);
  overflow: hidden;
}

.similitud-fill {
  height: 100%;
  background: linear-gradient(90deg, var(--accent-strong), var(--accent));
  border-radius: 999px;
  transition: width 0.4s ease;
}

.result-content {
  font-size: 13.5px;
  color: var(--text-muted);
  line-height: 1.6;
  max-height: 120px;
  overflow-y: auto;
}

@media (max-width: 700px) {
  .busq-options {
    grid-template-columns: 1fr;
  }
}
</style>
