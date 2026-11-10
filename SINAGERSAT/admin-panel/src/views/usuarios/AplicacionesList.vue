<script setup>
import { ref, onMounted } from 'vue'
import { listarAplicaciones, crearAplicacion, actualizarAplicacion, eliminarAplicacion } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const toast = useToastStore()
const apps = ref([]); const loading = ref(true)
const showForm = ref(false); const editItem = ref(null); const form = ref({ codigo: '', nombre: '', descripcion: '', activo: true })
const err = ref('')

const columns = [
  { key: 'id_aplicacion', label: 'ID' },
  { key: 'codigo', label: 'Código' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'activo', label: 'Estado' },
]

async function load() {
  loading.value = true
  try { const r = await listarAplicaciones(); apps.value = r.data || [] }
  catch (e) { console.error(e) }
  finally { loading.value = false }
}

function openCreate() { editItem.value = null; form.value = { codigo: '', nombre: '', descripcion: '', activo: true }; showForm.value = true }
function openEdit(row) { editItem.value = row; form.value = { codigo: row.codigo, nombre: row.nombre, descripcion: row.descripcion || '', activo: row.activo }; showForm.value = true }

async function save() {
  err.value = ''
  try {
    if (editItem.value) { await actualizarAplicacion(editItem.value.id_aplicacion, form.value); toast.success('Aplicación actualizada') }
    else { await crearAplicacion(form.value); toast.success('Aplicación creada') }
    showForm.value = false; load()
  } catch (e) { err.value = e.response?.data?.detail || 'Error' }
}

async function del(row) {
  try { await eliminarAplicacion(row.id_aplicacion); toast.success('Aplicación eliminada'); load() }
  catch (e) { toast.error(e.response?.data?.detail || 'Error') }
}

onMounted(load)
</script>

<template>
  <div>
    <div class="flex items-center justify-between mb-5">
      <div>
        <h1 class="text-xl font-bold text-slate-900">Aplicaciones</h1>
        <p class="text-sm text-slate-500">Módulos del sistema</p>
      </div>
      <button @click="openCreate" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nueva</button>
    </div>

    <div v-if="showForm" class="bg-white border border-slate-200 rounded-xl p-5 mb-5 max-w-md shadow-sm">
      <h2 class="font-semibold text-slate-800 mb-4">{{ editItem ? 'Editar' : 'Nueva' }} Aplicación</h2>
      <div class="space-y-3">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Código</label>
          <input v-model="form.codigo" placeholder="USERS" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nombre</label>
          <input v-model="form.nombre" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Descripción</label>
          <input v-model="form.descripcion" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <label class="flex items-center gap-3">
          <label class="relative inline-flex items-center cursor-pointer">
            <input v-model="form.activo" type="checkbox" class="sr-only peer" />
            <div class="w-9 h-5 bg-slate-200 rounded-full peer peer-checked:bg-emerald-500 peer-checked:after:translate-x-full after:content-[''] after:absolute after:top-0.5 after:left-[2px] after:bg-white after:rounded-full after:h-4 after:w-4 after:transition-all"></div>
          </label>
          <span class="text-sm text-slate-600">Activo</span>
        </label>
        <p v-if="err" class="text-rose-600 text-sm">{{ err }}</p>
        <div class="flex gap-2 pt-2">
          <button @click="save" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">Guardar</button>
          <button @click="showForm = false" class="text-slate-600 hover:bg-slate-100 px-4 py-2 rounded-lg text-sm font-medium transition-all">Cancelar</button>
        </div>
      </div>
    </div>

    <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
      <DataTable :columns="columns" :data="apps" :loading="loading" @delete="del">
        <template #cell-activo="{ value }">
          <span :class="value ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700'" class="text-xs font-medium px-2.5 py-0.5 rounded-full">{{ value ? 'Sí' : 'No' }}</span>
        </template>
        <template #actions="{ row }">
          <button @click="openEdit(row)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
        </template>
      </DataTable>
    </div>
  </div>
</template>
