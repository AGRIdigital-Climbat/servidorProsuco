<script setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { listarRoles, eliminarRol } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'

const router = useRouter(); const toast = useToastStore()
const roles = ref([]); const loading = ref(true)
const columns = [
  { key: 'id_rol', label: 'ID' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'descripcion', label: 'Descripción' },
]

async function load() {
  loading.value = true
  try { const r = await listarRoles(); roles.value = r.data || [] }
  catch (e) { console.error(e) }
  finally { loading.value = false }
}

async function del(row) {
  try { await eliminarRol(row.id_rol); toast.success('Rol eliminado'); load() }
  catch (e) { toast.error(e.response?.data?.detail || 'Error') }
}

onMounted(load)
</script>

<template>
  <div>
    <div class="flex items-center justify-between mb-5">
      <div>
        <h1 class="text-xl font-bold text-slate-900">Roles</h1>
        <p class="text-sm text-slate-500">Roles del sistema (1-7)</p>
      </div>
      <button @click="router.push('/usuarios/roles/nuevo')" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nuevo</button>
    </div>

    <div class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
      <DataTable :columns="columns" :data="roles" :loading="loading" @delete="del">
        <template #cell-id_rol="{ value }">
          <span class="bg-indigo-100 text-indigo-700 text-xs font-medium px-2.5 py-0.5 rounded-full">{{ value }}</span>
        </template>
        <template #actions="{ row }">
          <button @click="router.push(`/usuarios/roles/${row.id_rol}/editar`)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
        </template>
      </DataTable>
    </div>
  </div>
</template>
