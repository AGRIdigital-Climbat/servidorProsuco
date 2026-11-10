<script setup>
import { ref, onMounted } from 'vue'
import { listarProvincias, crearProvincia, actualizarProvincia, eliminarProvincia, listarDepartamentos } from '../../api/shared'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const toast = useToastStore()
const data = ref([]); const departamentos = ref([]); const loading = ref(true)
const showForm = ref(false); const editItem = ref(null); const form = ref({ nombre: '', codigo_ine: '', id_departamento: null })
const err = ref('')
const filtroDepto = ref('')
const columns = [
  { key: 'id', label: 'ID' },
  { key: 'codigo_ine', label: 'Código INE' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'id_departamento', label: 'Depto' },
]

async function load() {
  loading.value = true
  try {
    const params = {}
    if (filtroDepto.value) params.id_departamento = filtroDepto.value
    const [r, d] = await Promise.all([listarProvincias(params), listarDepartamentos()])
    data.value = r.data || []
    departamentos.value = d.data || []
  } catch (e) { console.error(e) }
  finally { loading.value = false }
}
function openCreate() { editItem.value = null; form.value = { nombre: '', codigo_ine: '', id_departamento: null }; showForm.value = true }
function openEdit(row) { editItem.value = row; form.value = { nombre: row.nombre, codigo_ine: row.codigo_ine || '', id_departamento: row.id_departamento }; showForm.value = true }
async function save() {
  err.value = ''
  try {
    if (editItem.value) { await actualizarProvincia(editItem.value.id, form.value); toast.success('Provincia actualizada') }
    else { await crearProvincia(form.value); toast.success('Provincia creada') }
    showForm.value = false; load()
  } catch (e) { err.value = e.response?.data?.detail || 'Error' }
}
async function del(row) { try { await eliminarProvincia(row.id); toast.success('Provincia eliminada'); load() } catch (e) { toast.error(e.response?.data?.detail || 'Error') } }
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
            <label class="block text-xs text-slate-500 mb-1">Departamento</label>
            <select v-model="filtroDepto" @change="load" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option value="">Todos</option>
              <option v-for="d in departamentos" :key="d.id" :value="d.id">{{ d.nombre }}</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <div class="flex-1 min-w-0">
      <div class="flex items-center justify-between mb-5">
        <div>
          <h1 class="text-xl font-bold text-slate-900">Provincias</h1>
          <p class="text-sm text-slate-500">Provincias por departamento</p>
        </div>
        <button @click="openCreate" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nueva</button>
      </div>

      <!-- Mobile filter (visible on small screens) -->
      <div class="lg:hidden flex gap-2 mb-4">
        <select v-model="filtroDepto" @change="load" class="flex-1 border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white">
          <option value="">Todos los departamentos</option>
          <option v-for="d in departamentos" :key="d.id" :value="d.id">{{ d.nombre }}</option>
        </select>
      </div>

      <div v-if="showForm" class="bg-white border border-slate-200 rounded-xl p-5 mb-5 max-w-md shadow-sm">
        <h2 class="font-semibold text-slate-800 mb-4">{{ editItem ? 'Editar' : 'Nueva' }} Provincia</h2>
        <div class="space-y-3">
          <div>
            <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Departamento</label>
            <select v-model.number="form.id_departamento" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option :value="null" disabled>Seleccionar</option>
              <option v-for="d in departamentos" :key="d.id" :value="d.id">{{ d.nombre }}</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Código INE</label>
            <input v-model="form.codigo_ine" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
          </div>
          <div>
            <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nombre</label>
            <input v-model="form.nombre" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
          </div>
          <p v-if="err" class="text-rose-600 text-sm">{{ err }}</p>
          <div class="flex gap-2 pt-2">
            <button @click="save" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm">Guardar</button>
            <button @click="showForm = false" class="text-slate-600 hover:bg-slate-100 px-4 py-2 rounded-lg text-sm font-medium transition-all">Cancelar</button>
          </div>
        </div>
      </div>

      <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="data" :loading="loading" @delete="del">
          <template #cell-id_departamento="{ value }">
            <span class="bg-slate-100 text-slate-600 text-xs font-medium px-2.5 py-0.5 rounded-full">{{ departamentos.find(d => d.id === value)?.nombre || value }}</span>
          </template>
          <template #actions="{ row }">
            <button @click="openEdit(row)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
