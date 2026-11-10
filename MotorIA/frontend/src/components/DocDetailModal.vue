<script setup>
import { ref } from "vue";
import { useModals } from "../composables/useModals.js";
import { useTipos } from "../composables/useTipos.js";
import { fechaHora } from "../utils/format.js";
import { api } from "../api.js";
import AppIcon from "./AppIcon.vue";
import QualityChart from "./QualityChart.vue";

const { state, closeDocDetail } = useModals();
const { claseDe } = useTipos();

const verificando = ref(false);
const verificacion = ref(null);

async function ejecutarVerificacion() {
  if (!state.docDetail.doc) return;
  verificando.value = true;
  verificacion.value = null;
  try {
    const r = await api.verificar(state.docDetail.doc.id);
    verificacion.value = await r.json();
  } catch {
    verificacion.value = { error: "Error al ejecutar la verificación" };
  } finally {
    verificando.value = false;
  }
}
</script>

<template>
  <Transition name="modal">
    <div v-if="state.docDetail.show" class="modal-overlay" @click.self="closeDocDetail">
      <div class="modal">
        <div class="modal-header">
          <h3>{{ state.docDetail.doc ? state.docDetail.doc.titulo : "Detalle del documento" }}</h3>
          <button class="modal-close" @click="closeDocDetail">
            <AppIcon name="x" :size="18" />
          </button>
        </div>

        <div class="modal-body">
          <div v-if="state.docDetail.loading" class="skeleton det-skel"></div>

          <p v-else-if="state.docDetail.error" class="empty-state">{{ state.docDetail.error }}</p>

          <template v-else-if="state.docDetail.doc">
            <table class="det-table">
              <tbody>
                <tr>
                  <td>Tipo</td>
                  <td>
                    <span v-if="state.docDetail.doc.tipo_nombre" class="badge" :class="claseDe(state.docDetail.doc.prioridad ?? state.docDetail.doc.tipo_id)">
                      {{ state.docDetail.doc.tipo_nombre }}
                    </span>
                    <span v-else class="muted">No asignado</span>
                  </td>
                </tr>
                <tr>
                  <td>Fecha de publicación</td>
                  <td>{{ state.docDetail.doc.fecha_publicacion || "No registrada" }}</td>
                </tr>
                <tr>
                  <td>Número de páginas</td>
                  <td>{{ state.docDetail.doc.num_paginas || "No registrado" }}</td>
                </tr>
                <tr>
                  <td>Secciones indexadas</td>
                  <td>{{ state.docDetail.doc.total_chunks || 0 }}</td>
                </tr>
                <tr>
                  <td>Fecha de carga</td>
                  <td>{{ fechaHora(state.docDetail.doc.creado_en) }}</td>
                </tr>
              </tbody>
            </table>

            <div v-if="state.docDetail.doc.metadatos && Object.keys(state.docDetail.doc.metadatos).length" class="det-section">
              <h4>Metadatos del PDF</h4>
              <div class="meta-tags">
                <span v-for="([k, v], i) in Object.entries(state.docDetail.doc.metadatos)" :key="i" class="meta-tag">
                  <strong>{{ k }}:</strong> {{ String(v) }}
                </span>
              </div>
            </div>

            <div class="det-section">
              <h4>Secciones indexadas</h4>
              <p v-if="!state.docDetail.chunks.length" class="empty-state">Este documento no tiene secciones.</p>
              <div v-else class="chunk-list">
                <div v-for="c in state.docDetail.chunks" :key="c.id" class="chunk-item">
                  <div class="chunk-head">
                    <span class="chunk-ord">#{{ c.orden + 1 }}</span>
                    <span v-if="c.numeral" class="badge">{{ c.numeral }}</span>
                  </div>
                  <div class="chunk-content">{{ c.contenido }}</div>
                </div>
              </div>
            </div>

            <div class="det-section verif-section">
              <div class="verif-row">
                <h4>Calidad de indexación</h4>
                <button class="btn btn--ghost btn--sm" @click="ejecutarVerificacion" :disabled="verificando">
                  <span v-if="verificando" class="spin"></span>
                  <AppIcon v-else name="zap" :size="14" />
                  {{ verificando ? "Verificando…" : "Verificar" }}
                </button>
              </div>

              <div v-if="verificacion && verificacion.total_queries > 0" class="verif-chart">
                <QualityChart :verificacion="verificacion" />
              </div>
              <p v-else-if="verificacion && verificacion.total_queries === 0" class="empty-state">No hay suficientes datos para evaluar la calidad.</p>
              <p v-else-if="verificacion && verificacion.error" class="result result--error">{{ verificacion.error }}</p>
            </div>
          </template>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.det-skel {
  height: 200px;
}

.det-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13.5px;
}

.det-table td {
  padding: 8px 4px;
  border-bottom: 1px solid var(--border-soft);
}

.det-table td:first-child {
  color: var(--text-dim);
  width: 42%;
}

.det-section {
  margin-top: 18px;
}

.det-section h4 {
  font-size: 11.5px;
  color: var(--text-muted);
  margin-bottom: 10px;
  text-transform: uppercase;
  letter-spacing: 0.6px;
}

.chunk-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
  max-height: 260px;
  overflow-y: auto;
}

.chunk-item {
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: 10px;
  padding: 10px 12px;
}

.chunk-head {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 6px;
}

.chunk-ord {
  font-family: var(--mono);
  font-size: 12px;
  font-weight: 700;
  color: var(--accent);
}

.chunk-content {
  font-size: 13px;
  color: var(--text-muted);
  line-height: 1.55;
}

.verif-section {
  border-top: 1px dashed var(--border);
  padding-top: 14px;
}

.verif-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
}

.verif-row h4 {
  margin-bottom: 0;
}

.verif-chart {
  background: var(--surface-2);
  border: 1px solid var(--border-soft);
  border-radius: var(--radius);
  padding: 16px;
}
</style>
