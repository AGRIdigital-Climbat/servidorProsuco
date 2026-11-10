<script setup>
import { ref, onMounted, watch } from "vue";
import { api } from "../api.js";
import { useModals } from "../composables/useModals.js";
import { useTipos } from "../composables/useTipos.js";
import { fechaCorta } from "../utils/format.js";
import AppIcon from "./AppIcon.vue";
import UploadModal from "./UploadModal.vue";

const { openDocDetail, openDocEdit, state } = useModals();
const { loadTipos, claseDe: tipoClase } = useTipos();

const docs = ref([]);
const cargando = ref(false);
const error = ref("");
const mostrarUpload = ref(false);

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

async function eliminar(id, titulo) {
  if (!confirm(`¿Eliminar "${titulo}" y todos sus secciones indexadas?`)) return;
  try {
    const r = await api.borrarDocumento(id);
    const d = await r.json();
    alert(d.mensaje || "Documento eliminado");
    await loadDocumentos();
  } catch {
    alert("Error al eliminar el documento");
  }
}

function onSubido() {
  mostrarUpload.value = false;
  loadDocumentos();
}

watch(
  () => state.docEdit.show,
  (show) => {
    if (!show) loadDocumentos();
  },
);

onMounted(async () => {
  await loadTipos();
  await loadDocumentos();
});
</script>

<template>
  <div class="tab-pane">
    <header class="tab-header">
      <div>
        <h3>Biblioteca de normas</h3>
        <div class="sub">{{ docs.length }} documento(s) en la base de conocimiento</div>
      </div>
      <div class="header-actions">
        <button class="btn btn--ghost" title="Actualizar" @click="loadDocumentos">
          <AppIcon name="refresh" :size="16" />
          Actualizar
        </button>
        <button class="btn btn--primary" @click="mostrarUpload = true">
          <AppIcon name="upload" :size="16" />
          Subir documento
        </button>
      </div>
    </header>

    <div v-if="cargando" class="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Tipo</th>
            <th>Título</th>
            <th class="col-num">Páginas</th>
            <th class="col-num">Secciones</th>
            <th class="col-num">Cargado</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="i in 4" :key="i">
            <td colspan="6"><div class="skeleton row-skel"></div></td>
          </tr>
        </tbody>
      </table>
    </div>

    <p v-else-if="error" class="empty-state">{{ error }}</p>

    <div v-else-if="!docs.length" class="empty-state">
      No hay documentos cargados. Use el botón "Subir documento" para incorporar la primera norma.
    </div>

    <div v-else class="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Tipo</th>
            <th>Título</th>
            <th class="col-num">Páginas</th>
            <th class="col-num">Secciones</th>
            <th class="col-num">Cargado</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="doc in docs" :key="doc.id">
            <td>
              <span class="badge" :class="tipoClase(doc.prioridad ?? doc.tipo_id)">{{ doc.tipo_nombre }}</span>
            </td>
            <td class="cell-titulo">
              <strong>{{ doc.titulo }}</strong>
              <span v-if="doc.fecha_publicacion" class="cell-sub">Pub. {{ doc.fecha_publicacion }}</span>
            </td>
            <td class="col-num">{{ doc.num_paginas || "—" }}</td>
            <td class="col-num">{{ doc.total_chunks }}</td>
            <td class="col-num">{{ fechaCorta(doc.creado_en) }}</td>
            <td>
              <div class="td-actions">
                <button class="btn btn--ghost btn--sm" title="Detalle" @click="openDocDetail(doc.id)">
                  <AppIcon name="eye" :size="15" />
                </button>
                <button class="btn btn--ghost btn--sm" title="Editar" @click="openDocEdit(doc)">
                  <AppIcon name="pencil" :size="15" />
                </button>
                <button class="btn btn--danger btn--sm" title="Eliminar" @click="eliminar(doc.id, doc.titulo)">
                  <AppIcon name="trash" :size="15" />
                </button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <UploadModal :show="mostrarUpload" @close="mostrarUpload = false" @subido="onSubido" />
  </div>
</template>

<style scoped>
.header-actions {
  display: flex;
  gap: 10px;
}

.td-actions {
  display: flex;
  justify-content: flex-end;
  gap: 4px;
}

.td-actions .btn {
  width: 32px;
  height: 32px;
  padding: 0;
  border-radius: 8px;
}

.td-actions .btn--danger {
  background: transparent;
  border: 1px solid transparent;
}

.col-num {
  text-align: center;
  width: 88px;
}

.cell-titulo {
  min-width: 220px;
}

.cell-titulo strong {
  display: block;
  font-size: 14px;
  line-height: 1.35;
}

.cell-sub {
  display: block;
  font-size: 11.5px;
  color: var(--text-dim);
  margin-top: 2px;
}

.row-skel {
  height: 24px;
}
</style>
