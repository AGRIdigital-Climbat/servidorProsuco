<script setup>
import { reactive, ref, watch } from "vue";
import { api } from "../api.js";
import { useModals } from "../composables/useModals.js";
import { useTipos } from "../composables/useTipos.js";
import AppIcon from "./AppIcon.vue";

const { state, closeDocEdit } = useModals();
const { activos, loadTipos } = useTipos();

const form = reactive({ titulo: "", tipo_id: "", fecha_publicacion: "" });
const guardando = ref(false);
const mensaje = ref(null);

watch(
  () => state.docEdit.doc,
  (doc) => {
    if (doc) {
      form.titulo = doc.titulo || "";
      form.tipo_id = doc.tipo_id ? String(doc.tipo_id) : "";
      form.fecha_publicacion = doc.fecha_publicacion || "";
      mensaje.value = null;
      loadTipos();
    }
  },
);

async function guardar() {
  if (!form.titulo.trim()) {
    mensaje.value = { tipo: "error", texto: "El título no puede estar vacío" };
    return;
  }

  guardando.value = true;
  mensaje.value = null;
  try {
    const r = await api.editarDocumento(state.docEdit.doc.id, {
      titulo: form.titulo.trim(),
      tipo_id: parseInt(form.tipo_id),
      fecha_publicacion: form.fecha_publicacion || null,
    });
    const d = await r.json();
    if (r.ok) {
      mensaje.value = { tipo: "success", texto: d.mensaje };
      setTimeout(() => closeDocEdit(), 700);
    } else {
      mensaje.value = { tipo: "error", texto: d.detail || "Error al actualizar" };
    }
  } catch {
    mensaje.value = { tipo: "error", texto: "Error de conexión" };
  } finally {
    guardando.value = false;
  }
}
</script>

<template>
  <Transition name="modal">
    <div v-if="state.docEdit.show" class="modal-overlay" @click.self="closeDocEdit">
      <div class="modal">
        <div class="modal-header">
          <h3>Editar documento</h3>
          <button class="modal-close" @click="closeDocEdit">
            <AppIcon name="x" :size="18" />
          </button>
        </div>

        <div class="modal-body">
          <div class="form-group">
            <label for="editTitulo">Título</label>
            <input id="editTitulo" v-model="form.titulo" class="form-control" />
          </div>
          <div class="form-group">
            <label for="editTipo">Tipo de norma</label>
            <select id="editTipo" v-model="form.tipo_id" class="form-control">
              <option value="">Seleccione…</option>
              <option v-for="t in activos" :key="t.id" :value="String(t.id)">{{ t.prioridad }} · {{ t.nombre }}</option>
            </select>
          </div>
          <div class="form-group">
            <label for="editFecha">Fecha de publicación</label>
            <input id="editFecha" v-model="form.fecha_publicacion" type="date" class="form-control" />
          </div>

          <div v-if="mensaje" class="result" :class="mensaje.tipo === 'success' ? 'result--success' : 'result--error'">
            {{ mensaje.texto }}
          </div>

          <div class="modal-actions">
            <button class="btn btn--ghost" @click="closeDocEdit">Cancelar</button>
            <button class="btn btn--primary" :disabled="guardando" @click="guardar">
              <span v-if="guardando" class="spin"></span>
              Guardar cambios
            </button>
          </div>
        </div>
      </div>
    </div>
  </Transition>
</template>
