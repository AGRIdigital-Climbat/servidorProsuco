<script setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { listarReportes, eliminarReporte, listarEstados } from '../../api/rea'
import { listarMunicipios } from '../../api/shared'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const router = useRouter(); const toast = useToastStore()
const reportes = ref([]); const estados = ref([]); const municipios = ref([]); const loading = ref(true); const total = ref(0)
const filtroEstado = ref(''); const filtroMunicipio = ref('')
const columns = [
  { key: 'id_reporte', label: 'ID' },
  { key: 'municipio', label: 'Municipio' },
  { key: 'comunidad', label: 'Comunidad' },
  { key: 'fecha_evento', label: 'Fecha' },
  { key: 'id_estado', label: 'Estado' },
]

async function load() {
  loading.value = true
  try {
    const params = {}
    if (filtroEstado.value) params.id_estado = filtroEstado.value
    if (filtroMunicipio.value) params.id_municipio = filtroMunicipio.value
    const [r, e, m] = await Promise.all([listarReportes(params), listarEstados(), listarMunicipios()])
    reportes.value = r.data || []
    total.value = r.total || reportes.value.length
    estados.value = e.data || []
    municipios.value = m.data || []
  } catch (e) { console.error(e) }
  finally { loading.value = false }
}

async function del(row) { try { await eliminarReporte(row.id_reporte); toast.success('Reporte eliminado'); load() } catch (e) { toast.error(e.response?.data?.detail || 'Error') } }

const estadoColors = {
  1: 'bg-yellow-100 text-yellow-700',
  2: 'bg-blue-100 text-blue-700',
  3: 'bg-emerald-100 text-emerald-700',
  4: 'bg-rose-100 text-rose-700',
}

function estadoNombre(id) {
  const e = estados.value.find(x => x.id_estado === id)
  return e ? e.nombre : id
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
            <select v-model.number="filtroEstado" @change="load" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option :value="''">Todos</option>
              <option v-for="e in estados" :key="e.id_estado" :value="e.id_estado">{{ e.nombre }}</option>
            </select>
          </div>
          <div>
            <label class="block text-xs text-slate-500 mb-1">Municipio</label>
            <select v-model.number="filtroMunicipio" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option :value="''">Todos</option>
              <option v-for="m in municipios" :key="m.id" :value="m.id">{{ m.nombre }}</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <div class="flex-1 min-w-0">
      <div class="flex items-center justify-between mb-5">
        <div>
          <h1 class="text-xl font-bold text-slate-900">Reportes REA</h1>
          <p class="text-sm text-slate-500">Reportes de eventos de emergencia</p>
        </div>
        <button @click="router.push('/rea/reportes/nuevo')" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nuevo</button>
      </div>

      <!-- Mobile filters -->
      <div class="lg:hidden flex flex-wrap gap-2 mb-4">
        <select v-model.number="filtroEstado" @change="load" class="flex-1 min-w-[120px] border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white">
          <option :value="''">Todos los estados</option>
          <option v-for="e in estados" :key="e.id_estado" :value="e.id_estado">{{ e.nombre }}</option>
        </select>
        <select v-model.number="filtroMunicipio" class="flex-1 min-w-[120px] border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white">
          <option :value="''">Todos los municipios</option>
          <option v-for="m in municipios" :key="m.id" :value="m.id">{{ m.nombre }}</option>
        </select>
        <button @click="load" class="bg-indigo-600 text-white px-4 py-2 rounded-lg text-sm">Filtrar</button>
      </div>

      <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="reportes" :loading="loading" :total="total" @delete="del">
          <template #cell-municipio="{ row }">
            <span class="text-slate-700">{{ row.municipio?.nombre || row.id_municipio }}</span>
          </template>
          <template #cell-id_estado="{ row }">
            <span :class="estadoColors[row.id_estado] || 'bg-slate-100 text-slate-700'" class="text-xs font-medium px-2.5 py-0.5 rounded-full">{{ estadoNombre(row.id_estado) }}</span>
          </template>
          <template #cell-fecha_evento="{ value }">
            <span class="text-xs font-mono text-slate-400">{{ value }}</span>
          </template>
          <template #actions="{ row }">
            <button @click="router.push(`/rea/reportes/${row.id_reporte}/editar`)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
