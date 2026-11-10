<script setup>
import { reactive, ref, watch } from "vue";
import { api } from "../api.js";
import { useModals } from "../composables/useModals.js";
import { useTipos } from "../composables/useTipos.js";
import AppIcon from "./AppIcon.vue";

const { state, closeTipoModal } = useModals();
const { loadTipos } = useTipos();

const form = reactive({ nombre: "", prioridad: 20, activo: true });
const guardando = ref(false);
const mensaje = ref(null);

watch(
  () => [state.tipo.show, state.tipo.tipo],
  ([show, tipo]) => {
    if (show) {
      if (tipo) {
        form.nombre = tipo.nombre;
        form.prioridad = tipo.prioridad;
        form.activo = tipo.activo !== false;
      } else {
        form.nombre = "";
        form.prioridad = 20;
        form.activo = true;
      }
      mensaje.value = null;
    }
  },
);

async function guardar() {
  if (!form.nombre.trim()) {
    mensaje.value = { tipo: "error", texto: "El nombre no puede estar vacío" };
    return;
  }

  guardando.value = true;
  mensaje.value = null;
  try {
    let r;
    if (state.tipo.tipo) {
      r = await api.editarTipo(state.tipo.tipo.id, {
        nombre: form.nombre.trim(),
        prioridad: form.prioridad,
        activo: form.activo,
      });
    } else {
      r = await api.crearTipo({ nombre: form.nombre.trim(), prioridad: form.prioridad });
    }
    const d = await r.json();
    if (r.ok) {
      mensaje.value = { tipo: "success", texto: d.mensaje };
      await loadTipos();
      setTimeout(() => closeTipoModal(), 700);
    } else {
      mensaje.value = { tipo: "error", texto: d.detail || "Error al guardar" };
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
    <div v-if="state.tipo.show" class="modal-overlay" @click.self="closeTipoModal">
      <div class="modal">
        <div class="modal-header">
          <h3>{{ state.tipo.tipo ? "Editar tipo de norma" : "Nuevo tipo de norma" }}</h3>
          <button class="modal-close" @click="closeTipoModal">
            <AppIcon name="x" :size="18" />
          </button>
        </div>

        <div class="modal-body">
          <div class="form-group">
            <label for="tipoNombre">Nombre</label>
            <input id="tipoNombre" v-model="form.nombre" class="form-control" placeholder="Ej: Ley Municipal" />
          </div>
          <div class="form-group">
            <label for="tipoPrioridad">Prioridad (1 = mayor jerarquía)</label>
            <input id="tipoPrioridad" v-model.number="form.prioridad" type="number" min="1" class="form-control" />
          </div>
          <div class="form-group">
            <label class="check-label">
              <input v-model="form.activo" type="checkbox" />
              Tipo activo (visible en el chat y en subir documentos)
            </label>
          </div>

          <div v-if="mensaje" class="result" :class="mensaje.tipo === 'success' ? 'result--success' : 'result--error'">
            {{ mensaje.texto }}
          </div>

          <div class="modal-actions">
            <button class="btn btn--ghost" @click="closeTipoModal">Cancelar</button>
            <button class="btn btn--primary" :disabled="guardando" @click="guardar">
              <span v-if="guardando" class="spin"></span>
              Guardar
            </button>
          </div>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.check-label {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 500;
  font-size: 13.5px;
  color: var(--text);
}

.check-label input {
  accent-color: var(--accent);
  width: 16px;
  height: 16px;
}
</style>
