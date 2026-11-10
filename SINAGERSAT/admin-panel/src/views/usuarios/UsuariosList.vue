<script setup>
import { ref, onMounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { listarUsuarios, eliminarUsuario } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const router = useRouter()
const toast = useToastStore()
const usuarios = ref([]); const loading = ref(true); const total = ref(0)
const filtroActivo = ref('')

const columns = [
  { key: 'id_usuario', label: 'ID' },
  { key: 'usuario', label: 'Usuario' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'celular', label: 'Celular' },
  { key: 'id_rol', label: 'Rol' },
  { key: 'activo', label: 'Estado' },
]

async function load() {
  loading.value = true
  try {
    const params = {}
    if (filtroActivo.value !== '') params.activo = filtroActivo.value === 'true'
    const r = await listarUsuarios(params)
    usuarios.value = r.data || []
    total.value = r.total || usuarios.value.length
  } catch (e) { console.error(e) }
  finally { loading.value = false }
}

async function del(row) {
  try {
    await eliminarUsuario(row.id_usuario)
    toast.success('Usuario eliminado')
    load()
  } catch (e) { toast.error(e.response?.data?.detail || 'Error') }
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
              <option value="">Todos</option>
              <option value="true">Activos</option>
              <option value="false">Inactivos</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <!-- Content -->
    <div class="flex-1 min-w-0">
      <div class="flex items-center justify-between mb-5">
        <div>
          <h1 class="text-xl font-bold text-slate-900">Usuarios</h1>
          <p class="text-sm text-slate-500">Gestión de usuarios del sistema</p>
        </div>
        <button @click="router.push('/usuarios/nuevo')" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all duration-200">+ Nuevo</button>
      </div>

      <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="usuarios" :loading="loading" :total="total" @delete="del">
          <template #cell-activo="{ value }">
            <span :class="value ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700'" class="text-xs font-medium px-2.5 py-0.5 rounded-full">{{ value ? 'Activo' : 'Inactivo' }}</span>
          </template>
          <template #cell-id_rol="{ row }">
            <span class="bg-indigo-100 text-indigo-700 text-xs font-medium px-2.5 py-0.5 rounded-full">{{ row.rol?.nombre || row.id_rol }}</span>
          </template>
          <template #actions="{ row }">
            <button @click="router.push(`/usuarios/${row.id_usuario}/editar`)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
