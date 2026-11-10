<script setup>
import { reactive, ref, computed } from "vue";
import { api } from "../api.js";
import { useTipos } from "../composables/useTipos.js";
import AppIcon from "./AppIcon.vue";
import QualityChart from "./QualityChart.vue";

defineProps({
  show: { type: Boolean, default: false },
});

const emit = defineEmits(["close", "subido"]);

const { activos } = useTipos();

const form = reactive({
  titulo: "",
  tipo_id: "",
  fecha_publicacion: "",
  archivo: null,
});

const enviando = ref(false);
const resultado = ref(null);
const metadatos = ref([]);
const verificacion = ref(null);
const arrastrando = ref(false);
const fileInput = ref(null);

const archivoNombre = computed(() => form.archivo?.name || "");

function onArchivo(e) {
  form.archivo = e.target.files[0] || null;
  resultado.value = null;
}

function onDrop(e) {
  arrastrando.value = false;
  const f = e.dataTransfer?.files?.[0];
  if (f) {
    form.archivo = f;
    resultado.value = null;
  }
}

function elegirArchivo() {
  fileInput.value?.click();
}

async function onSubmit() {
  resultado.value = null;
  metadatos.value = [];
  verificacion.value = null;

  if (!form.titulo.trim()) {
    resultado.value = { tipo: "error", texto: "El título es obligatorio" };
    return;
  }
  if (!form.tipo_id) {
    resultado.value = { tipo: "error", texto: "Seleccione un tipo de norma" };
    return;
  }
  if (!form.archivo) {
    resultado.value = { tipo: "error", texto: "Seleccione un archivo PDF" };
    return;
  }

  enviando.value = true;
  try {
    const fd = new FormData();
    fd.append("titulo", form.titulo.trim());
    fd.append("tipo_id", form.tipo_id);
    fd.append("fecha_publicacion", form.fecha_publicacion || "");
    fd.append("archivo", form.archivo);

    const r = await api.subir(fd);
    const d = await r.json();

    if (r.ok) {
      resultado.value = {
        tipo: "success",
        texto: `"${d.titulo}" procesado: ${d.total_chunks} secciones y ${d.num_paginas} páginas indexadas.`,
      };

      if (d.verificacion) {
        verificacion.value = d.verificacion;
      }

      const det = await (await api.documento(d.documento_id)).json();
      if (det.metadatos && Object.keys(det.metadatos).length) {
        metadatos.value = Object.entries(det.metadatos);
      }
      form.titulo = "";
      form.tipo_id = "";
      form.fecha_publicacion = "";
      form.archivo = null;
      if (fileInput.value) fileInput.value.value = "";
      emit("subido", d.documento_id);
    } else {
      resultado.value = { tipo: "error", texto: d.detail || "Error al procesar el documento" };
    }
  } catch (err) {
    resultado.value = { tipo: "error", texto: "Error de conexión: " + err.message };
  } finally {
    enviando.value = false;
  }
}
</script>

<template>
  <Transition name="modal">
    <div v-if="show" class="modal-overlay" @click.self="emit('close')">
      <div class="modal upload-modal">
        <div class="modal-header">
          <h3>Subir documento normativo</h3>
          <button class="modal-close" @click="emit('close')">
            <AppIcon name="x" :size="18" />
          </button>
        </div>

        <div class="modal-body">
          <p class="form-hint">
            El sistema extrae el texto del PDF, lo organiza en secciones y lo incorpora a la
            biblioteca para responder consultas jurídicas.
          </p>

          <form @submit.prevent="onSubmit">
            <div class="form-group">
              <label for="upTitulo">Título del documento</label>
              <input id="upTitulo" v-model="form.titulo" class="form-control" placeholder="Ej: Ley de Gestión de Riesgos" />
            </div>

            <div class="form-row">
              <div class="form-group">
                <label for="upTipo">Tipo de norma</label>
                <select id="upTipo" v-model="form.tipo_id" class="form-control">
                  <option value="">Seleccione…</option>
                  <option v-for="t in activos" :key="t.id" :value="t.id">{{ t.prioridad }} · {{ t.nombre }}</option>
                </select>
              </div>
              <div class="form-group">
                <label for="upFecha">Fecha de publicación</label>
                <input id="upFecha" v-model="form.fecha_publicacion" type="date" class="form-control" />
              </div>
            </div>

            <div class="form-group">
              <label>Archivo PDF</label>
              <input ref="fileInput" type="file" accept=".pdf" class="hidden-input" @change="onArchivo" />
              <button
                type="button"
                class="dropzone"
                :class="{ dragging: arrastrando, filled: form.archivo }"
                @click="elegirArchivo"
                @dragover.prevent="arrastrando = true"
                @dragleave="arrastrando = false"
                @drop.prevent="onDrop"
              >
                <AppIcon name="upload" :size="22" />
                <template v-if="form.archivo">
                  <strong class="dz-name">{{ archivoNombre }}</strong>
                  <span class="dz-sub">Listo para procesar</span>
                </template>
                <template v-else>
                  <strong>Arrastre el PDF aquí</strong>
                  <span class="dz-sub">o haga clic para seleccionar un archivo</span>
                </template>
              </button>
            </div>

            <button class="btn btn--primary btn--block" type="submit" :disabled="enviando">
              <span v-if="enviando" class="spin"></span>
              <AppIcon v-else name="upload" :size="16" />
              {{ enviando ? "Procesando…" : "Subir y procesar" }}
            </button>
          </form>

          <div v-if="resultado" class="result" :class="resultado.tipo === 'success' ? 'result--success' : 'result--error'">
            {{ resultado.texto }}
          </div>

          <div v-if="metadatos.length" class="meta-section">
            <p class="meta-title">Metadatos detectados del PDF:</p>
            <div class="meta-tags">
              <span v-for="([k, v], i) in metadatos" :key="i" class="meta-tag">
                <strong>{{ k }}:</strong> {{ String(v) }}
              </span>
            </div>
          </div>

          <div v-if="verificacion && verificacion.total_queries > 0" class="quality-section">
            <div class="quality-title">
              <AppIcon name="zap" :size="16" />
              <strong>Calidad de indexación</strong>
              <span class="quality-count">{{ verificacion.total_queries }} consultas de prueba</span>
            </div>
            <QualityChart :verificacion="verificacion" />
          </div>

          <div class="modal-actions">
            <button class="btn btn--ghost" @click="emit('close')">Cerrar</button>
          </div>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.upload-modal {
  width: min(620px, calc(100vw - 40px));
}

.form-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 14px;
}

.btn--block {
  width: 100%;
  padding: 12px;
}

.hidden-input {
  display: none;
}

.dropzone {
  width: 100%;
  min-height: 96px;
  border: 1.5px dashed var(--border);
  border-radius: var(--radius);
  background: var(--surface-2);
  color: var(--text-muted);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  padding: 16px;
  transition: border-color 0.15s, background 0.15s, color 0.15s;
}

.dropzone:hover,
.dropzone.dragging {
  border-color: var(--accent);
  background: var(--accent-soft);
  color: var(--accent);
}

.dropzone.filled {
  border-color: var(--success);
  background: var(--success-soft);
  color: #166534;
}

.dropzone strong {
  font-size: 13.5px;
}

.dz-sub {
  font-size: 12px;
  opacity: 0.85;
}

.dz-name {
  max-width: 90%;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.meta-section,
.quality-section {
  margin-top: 18px;
  padding-top: 16px;
  border-top: 1px dashed var(--border);
}

.meta-title {
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text-muted);
}

.quality-title {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 14px;
  color: var(--text);
}

.quality-title .app-icon {
  color: var(--accent);
}

.quality-count {
  margin-left: auto;
  font-size: 11px;
  color: var(--text-dim);
  background: var(--surface-3);
  padding: 3px 8px;
  border-radius: 6px;
}

@media (max-width: 640px) {
  .form-row {
    grid-template-columns: 1fr;
  }
}
</style>
