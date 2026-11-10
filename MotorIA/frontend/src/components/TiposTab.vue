<script setup>
import { ref, onMounted, watch } from "vue";
import { api } from "../api.js";
import { useModals } from "../composables/useModals.js";
import { useTipos } from "../composables/useTipos.js";
import AppIcon from "./AppIcon.vue";

const { openTipoModal, state } = useModals();
const { tipos, loadTipos, claseDe } = useTipos();

const todos = ref([]);
const cargando = ref(false);
const error = ref("");

function nivel(prioridad) {
  if (prioridad === 1) return "Constitución";
  if (prioridad <= 3) return "Nacional";
  if (prioridad <= 5) return "Decreto / RM";
  if (prioridad <= 8) return "Departamental";
  if (prioridad <= 13) return "Reglamento / Guía";
  return "Municipal";
}

async function loadAdmin() {
  cargando.value = true;
  error.value = "";
  try {
    const r = await api.tipos(true);
    const d = await r.json();
    todos.value = d.tipos || [];
  } catch {
    error.value = "Error al cargar los tipos de norma";
  } finally {
    cargando.value = false;
  }
}

function nuevoTipo() {
  openTipoModal(null);
}

async function borrar(t) {
  if (!confirm(`¿Eliminar o desactivar el tipo "${t.nombre}"?`)) return;
  try {
    const r = await api.borrarTipo(t.id);
    const d = await r.json();
    alert(d.mensaje);
    await loadTipos();
    await loadAdmin();
  } catch {
    alert("Error al eliminar el tipo");
  }
}

watch(
  () => state.tipo.show,
  (show) => {
    if (!show) loadAdmin();
  },
);

onMounted(async () => {
  await loadTipos();
  await loadAdmin();
});
</script>

<template>
  <div class="tab-pane">
    <header class="tab-header">
      <div>
        <h3>Tipos de norma</h3>
        <div class="sub">Jerarquía de la normativa boliviana · la prioridad 1 es la de mayor jerarquía</div>
      </div>
      <button class="btn btn--primary" @click="nuevoTipo">
        <AppIcon name="plus" :size="16" />
        Nuevo tipo
      </button>
    </header>

    <div class="jerarquia">
      <div class="lvl">
        <span class="lvl-dot" style="background: var(--lvl-constitucion)"></span> Constitución
      </div>
      <div class="lvl">
        <span class="lvl-dot" style="background: var(--lvl-ley)"></span> Leyes nacionales / tratados
      </div>
      <div class="lvl">
        <span class="lvl-dot" style="background: var(--lvl-decreto)"></span> Decretos / resoluciones supremas
      </div>
      <div class="lvl">
        <span class="lvl-dot" style="background: var(--lvl-departamental)"></span> Normativa departamental
      </div>
      <div class="lvl">
        <span class="lvl-dot" style="background: var(--lvl-municipal)"></span> Normativa municipal
      </div>
    </div>

    <div v-if="cargando" class="skeleton table-skel"></div>
    <p v-else-if="error" class="empty-state">{{ error }}</p>

    <div v-else class="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Prioridad</th>
            <th>Tipo de norma</th>
            <th>Nivel</th>
            <th>Estado</th>
            <th style="text-align: right">Acciones</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="t in todos" :key="t.id" :class="{ 'row-inactivo': !t.activo }">
            <td class="prio-cell">
              <span class="prio-badge">{{ t.prioridad }}</span>
            </td>
            <td>
              <span class="badge" :class="claseDe(t.prioridad)">{{ t.nombre }}</span>
            </td>
            <td class="muted">{{ nivel(t.prioridad) }}</td>
            <td>
              <span class="badge" :class="t.activo ? 'badge--ok' : 'badge--no'">
                {{ t.activo ? "Activo" : "Inactivo" }}
              </span>
            </td>
            <td>
              <div class="td-actions">
                <button class="btn btn--ghost btn--sm" @click="openTipoModal(t)">Editar</button>
                <button class="btn btn--danger btn--sm" @click="borrar(t)">Eliminar</button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<style scoped>
.jerarquia {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-bottom: 18px;
}

.lvl {
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 12px;
  color: var(--text-muted);
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 999px;
  padding: 6px 12px;
}

.lvl-dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
}

.table-skel {
  height: 200px;
}

.prio-cell {
  width: 60px;
}

.prio-badge {
  display: inline-grid;
  place-items: center;
  width: 26px;
  height: 26px;
  border-radius: 8px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  font-family: var(--mono);
  font-size: 12.5px;
  font-weight: 700;
  color: var(--accent);
}
</style>
