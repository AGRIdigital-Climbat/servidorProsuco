<script setup>
import { ref, onMounted } from 'vue'
import { listarAuditoria } from '../../api/usuarios'
import DataTable from '../../components/DataTable.vue'

const data = ref([]); const loading = ref(true); const total = ref(0)
const filtroAccion = ref('')

const columns = [
  { key: 'id_auditoria', label: 'ID' },
  { key: 'usuario', label: 'Usuario' },
  { key: 'accion', label: 'Acción' },
  { key: 'app_codigo', label: 'App' },
  { key: 'fecha', label: 'Fecha' },
  { key: 'ip_origen', label: 'IP' },
]

const accionColors = {
  CREATE: 'bg-emerald-100 text-emerald-700',
  UPDATE: 'bg-blue-100 text-blue-700',
  DELETE: 'bg-rose-100 text-rose-700',
  LOGIN: 'bg-indigo-100 text-indigo-700',
  READ: 'bg-slate-100 text-slate-700',
}

async function load() {
  loading.value = true
  try {
    const params = { limit: 100 }
    if (filtroAccion.value) params.accion = filtroAccion.value
    const r = await listarAuditoria(params)
    data.value = r.data || []
    total.value = r.total || data.value.length
  } catch (e) { console.error(e) }
  finally { loading.value = false }
}

onMounted(load)
</script>

<template>
  <div class="flex gap-5">
    <!-- Filter sidebar -->
    <aside class="w-56 shrink-0 hidden lg:block">
      <div class="bg-white border border-slate-200 rounded-xl p-4 shadow-sm sticky top-20">
        <h3 class="text-xs font-semibold text-slate-500 uppercase tracking-wider mb-3">Filtros</h3>
        <div class="space-y-3">
          <div>
            <label class="block text-xs text-slate-500 mb-1">Acción</label>
            <select v-model="filtroAccion" @change="load" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option value="">Todas</option>
              <option value="CREATE">CREATE</option>
              <option value="UPDATE">UPDATE</option>
              <option value="DELETE">DELETE</option>
              <option value="LOGIN">LOGIN</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <div class="flex-1 min-w-0">
      <div class="mb-5">
        <h1 class="text-xl font-bold text-slate-900">Auditoría</h1>
        <p class="text-sm text-slate-500">Registro de actividades del sistema</p>
      </div>
      <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="data" :loading="loading" :total="total">
          <template #cell-accion="{ value }">
            <span :class="accionColors[value] || 'bg-slate-100 text-slate-700'" class="text-xs font-medium px-2.5 py-0.5 rounded-full">{{ value }}</span>
          </template>
          <template #cell-fecha="{ value }">
            <span class="text-xs font-mono text-slate-400">{{ value ? new Date(value).toLocaleString('es-BO') : '' }}</span>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
