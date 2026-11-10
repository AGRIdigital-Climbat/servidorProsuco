<script setup>
import { ref, onMounted } from 'vue'
import { listarDepartamentos, crearDepartamento, actualizarDepartamento, eliminarDepartamento } from '../../api/shared'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const toast = useToastStore()
const data = ref([]); const loading = ref(true)
const showForm = ref(false); const editItem = ref(null); const form = ref({ nombre: '', codigo_ine: '' })
const err = ref('')
const columns = [
  { key: 'id', label: 'ID' },
  { key: 'codigo_ine', label: 'Código INE' },
  { key: 'nombre', label: 'Nombre' },
]

async function load() {
  loading.value = true
  try { const r = await listarDepartamentos(); data.value = r.data || [] }
  catch (e) { console.error(e) }
  finally { loading.value = false }
}
function openCreate() { editItem.value = null; form.value = { nombre: '', codigo_ine: '' }; showForm.value = true }
function openEdit(row) { editItem.value = row; form.value = { nombre: row.nombre, codigo_ine: row.codigo_ine || '' }; showForm.value = true }
async function save() {
  err.value = ''
  try {
    if (editItem.value) { await actualizarDepartamento(editItem.value.id, form.value); toast.success('Departamento actualizado') }
    else { await crearDepartamento(form.value); toast.success('Departamento creado') }
    showForm.value = false; load()
  } catch (e) { err.value = e.response?.data?.detail || 'Error' }
}
async function del(row) {
  try { await eliminarDepartamento(row.id); toast.success('Departamento eliminado'); load() }
  catch (e) { toast.error(e.response?.data?.detail || 'Error') }
}
onMounted(load)
</script>

<template>
  <div>
    <div class="flex items-center justify-between mb-5">
      <div>
        <h1 class="text-xl font-bold text-slate-900">Departamentos</h1>
        <p class="text-sm text-slate-500">9 departamentos de Bolivia</p>
      </div>
      <button @click="openCreate" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nuevo</button>
    </div>

    <div v-if="showForm" class="bg-white border border-slate-200 rounded-xl p-5 mb-5 max-w-md shadow-sm">
      <h2 class="font-semibold text-slate-800 mb-4">{{ editItem ? 'Editar' : 'Nuevo' }} Departamento</h2>
      <div class="space-y-3">
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
          <button @click="save" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">Guardar</button>
          <button @click="showForm = false" class="text-slate-600 hover:bg-slate-100 px-4 py-2 rounded-lg text-sm font-medium transition-all">Cancelar</button>
        </div>
      </div>
    </div>

    <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
      <DataTable :columns="columns" :data="data" :loading="loading" @delete="del">
        <template #actions="{ row }">
          <button @click="openEdit(row)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
        </template>
      </DataTable>
    </div>
  </div>
</template>
