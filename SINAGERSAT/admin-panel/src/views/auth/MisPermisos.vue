<script setup>
import { ref, onMounted } from 'vue'
import { misPermisos } from '../../api/auth'

const data = ref(null); const loading = ref(true); const err = ref('')

onMounted(async () => {
  try {
    const r = await misPermisos()
    data.value = r.data
  } catch (e) {
    err.value = e.response?.data?.detail || e.response?.data?.message || 'Error al obtener permisos'
  }
  finally { loading.value = false }
})
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚿</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">Mis Permisos</h1>
        <p class="text-sm text-slate-500">Permisos asignados a mi usuario</p>
      </div>
    </div>

    <div v-if="loading" class="space-y-3">
      <div class="skeleton h-8 w-48"></div>
      <div class="skeleton h-40 w-full max-w-xl"></div>
    </div>
    <div v-else-if="err" class="text-rose-600 bg-rose-50 border border-rose-200 p-4 rounded-xl flex items-center gap-2">✕ {{ err }}</div>
    <div v-else-if="data" class="max-w-2xl space-y-5">
      <div class="bg-white border border-slate-200 rounded-xl p-5 shadow-sm">
        <div class="flex items-center gap-3 mb-4">
          <div class="w-10 h-10 rounded-full bg-indigo-100 text-indigo-700 flex items-center justify-center text-sm font-bold">{{ (data.nombre || data.usuario || '?')[0] }}</div>
          <div>
            <h2 class="font-semibold text-slate-800">{{ data.nombre || data.usuario }}</h2>
            <span class="inline-block bg-indigo-100 text-indigo-700 text-xs font-semibold px-2.5 py-0.5 rounded-full">Rol {{ data.id_rol }}</span>
          </div>
        </div>
        <div class="grid grid-cols-2 gap-3 text-sm">
          <div class="bg-slate-50 rounded-lg p-3">
            <span class="text-xs text-slate-400 uppercase tracking-wide">ID</span>
            <div class="font-mono text-slate-800 mt-0.5">{{ data.id_usuario }}</div>
          </div>
          <div class="bg-slate-50 rounded-lg p-3">
            <span class="text-xs text-slate-400 uppercase tracking-wide">Usuario</span>
            <div class="font-medium text-slate-800 mt-0.5">{{ data.usuario }}</div>
          </div>
        </div>
      </div>

      <div class="bg-white border border-slate-200 rounded-xl p-5 shadow-sm">
        <h2 class="font-semibold text-slate-800 mb-4">Permisos</h2>
        <div v-if="!data.permisos || Object.keys(data.permisos).length === 0" class="text-slate-400 text-sm">Sin permisos asignados</div>
        <div v-else class="space-y-2">
          <div v-for="(acciones, app) in data.permisos" :key="app"
            class="flex items-center justify-between bg-slate-50 rounded-lg px-4 py-3 border border-slate-100">
            <span class="font-medium text-slate-700 text-sm">{{ app }}</span>
            <div class="flex flex-wrap gap-1.5">
              <span v-for="a in acciones" :key="a" class="text-xs bg-indigo-100 text-indigo-700 font-medium px-2.5 py-1 rounded-full">{{ a }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
