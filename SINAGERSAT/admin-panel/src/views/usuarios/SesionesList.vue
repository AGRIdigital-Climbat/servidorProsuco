<script setup>
import { ref, onMounted } from 'vue'
import { listarSesiones, listarUsuarios } from '../../api/usuarios'
import DataTable from '../../components/DataTable.vue'

const data = ref([]); const loading = ref(true); const total = ref(0)
const userMap = ref({}); const filtroActivo = ref('')

const columns = [
  { key: 'id_sesion', label: 'ID' },
  { key: 'id_usuario', label: 'Usuario' },
  { key: 'fecha_inicio', label: 'Inicio' },
  { key: 'fecha_expiracion', label: 'Expira' },
  { key: 'activo', label: 'Estado' },
]

async function load() {
  loading.value = true
  try {
    const params = { limit: 100 }
    if (filtroActivo.value !== '') params.activo = filtroActivo.value === 'true'
    const [r, u] = await Promise.all([listarSesiones(params), listarUsuarios({})])
    data.value = r.data || []
    total.value = r.total || data.value.length
    for (const usr of (u.data || [])) userMap.value[usr.id_usuario] = usr.usuario || usr.nombre
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
            <label class="block text-xs text-slate-500 mb-1">Estado</label>
            <select v-model="filtroActivo" @change="load" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option value="">Todas</option>
              <option value="true">Activas</option>
              <option value="false">Expiradas</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <div class="flex-1 min-w-0">
      <div class="mb-5">
        <h1 class="text-xl font-bold text-slate-900">Sesiones</h1>
        <p class="text-sm text-slate-500">Sesiones activas de usuarios</p>
      </div>
      <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="data" :loading="loading" :total="total">
          <template #cell-id_usuario="{ value }">
            <span class="text-slate-700">{{ userMap[value] || value }}</span>
          </template>
          <template #cell-activo="{ value }">
            <span :class="value ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700'" class="text-xs font-medium px-2.5 py-0.5 rounded-full">{{ value ? 'Activa' : 'Expirada' }}</span>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
