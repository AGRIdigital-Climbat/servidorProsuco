<script setup>
import { ref, onMounted } from 'vue'
import { listarEstados, crearEstado, actualizarEstado, eliminarEstado } from '../../api/rea'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const toast = useToastStore()
const data = ref([]); const loading = ref(true)
const showForm = ref(false); const editItem = ref(null); const form = ref({ nombre: '', descripcion: '', orden: 0 })
const err = ref('')
const columns = [
  { key: 'id_estado', label: 'ID' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'orden', label: 'Orden' },
]

async function load() {
  loading.value = true
  try { const r = await listarEstados(); data.value = r.data || [] }
  catch (e) { console.error(e) }
  finally { loading.value = false }
}
function openCreate() { editItem.value = null; form.value = { nombre: '', descripcion: '', orden: 0 }; showForm.value = true }
function openEdit(row) { editItem.value = row; form.value = { nombre: row.nombre, descripcion: row.descripcion || '', orden: row.orden }; showForm.value = true }
async function save() {
  err.value = ''
  try {
    if (editItem.value) { await actualizarEstado(editItem.value.id_estado, form.value); toast.success('Estado actualizado') }
    else { await crearEstado(form.value); toast.success('Estado creado') }
    showForm.value = false; load()
  } catch (e) { err.value = e.response?.data?.detail || 'Error' }
}
async function del(row) { try { await eliminarEstado(row.id_estado); toast.success('Estado eliminado'); load() } catch (e) { toast.error(e.response?.data?.detail || 'Error') } }
onMounted(load)
</script>

<template>
  <div>
    <div class="flex items-center justify-between mb-5">
      <div>
        <h1 class="text-xl font-bold text-slate-900">Estados de Reporte</h1>
        <p class="text-sm text-slate-500">Estados para el flujo de reportes REA</p>
      </div>
      <button @click="openCreate" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nuevo</button>
    </div>

    <div v-if="showForm" class="bg-white border border-slate-200 rounded-xl p-5 mb-5 max-w-md shadow-sm">
      <h2 class="font-semibold text-slate-800 mb-4">{{ editItem ? 'Editar' : 'Nuevo' }} Estado</h2>
      <div class="space-y-3">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nombre</label>
          <input v-model="form.nombre" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Descripción</label>
          <input v-model="form.descripcion" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Orden</label>
          <input v-model.number="form.orden" type="number" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
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
        <template #actions="{ row }">
          <button @click="openEdit(row)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
        </template>
      </DataTable>
    </div>
  </div>
</template>
