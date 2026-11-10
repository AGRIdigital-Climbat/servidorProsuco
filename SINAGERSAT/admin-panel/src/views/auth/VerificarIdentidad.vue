<script setup>
import { ref, onMounted } from 'vue'
import { verificarIdentidad } from '../../api/auth'
import { listarDepartamentos, listarProvincias, listarMunicipios } from '../../api/shared'

const form = ref({ carnet: '', celular: '', id_departamento: null, id_provincia: null, id_municipio: null, comunidad: '' })
const departamentos = ref([]); const provincias = ref([]); const municipios = ref([])
const result = ref(null); const err = ref(''); const loading = ref(false)

onMounted(async () => {
  try {
    const [d, p, m] = await Promise.all([listarDepartamentos(), listarProvincias(), listarMunicipios()])
    departamentos.value = d.data || []
    provincias.value = p.data || []
    municipios.value = m.data || []
  } catch (e) { console.error(e) }
})

async function submit() {
  err.value = ''; result.value = null; loading.value = true
  try {
    const r = await verificarIdentidad(form.value)
    result.value = r.data
  } catch (e) { err.value = e.response?.data?.detail || 'No se encontró coincidencia' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚿</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">Verificar Identidad</h1>
        <p class="text-sm text-slate-500">Validar datos de un usuario en el sistema</p>
      </div>
    </div>

    <form @submit.prevent="submit" class="max-w-lg bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div class="grid grid-cols-2 gap-4">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Carnet *</label>
          <input v-model="form.carnet" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Celular *</label>
          <input v-model="form.celular" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Departamento</label>
          <select v-model.number="form.id_departamento" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null">Seleccionar</option>
            <option v-for="d in departamentos" :key="d.id" :value="d.id">{{ d.nombre }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Provincia</label>
          <select v-model.number="form.id_provincia" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null">Seleccionar</option>
            <option v-for="p in provincias" :key="p.id" :value="p.id">{{ p.nombre }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Municipio</label>
          <select v-model.number="form.id_municipio" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null">Seleccionar</option>
            <option v-for="m in municipios" :key="m.id" :value="m.id">{{ m.nombre }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Comunidad *</label>
          <input v-model="form.comunidad" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
      </div>
      <p v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ err }}</p>
      <div v-if="result" class="text-emerald-700 bg-emerald-50 border border-emerald-200 p-3 rounded-lg text-sm flex items-center gap-2">✓ Usuario ID: {{ result.id_usuario }} — identidad verificada</div>
      <button type="submit" :disabled="loading"
        class="bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
        <span v-if="loading" class="inline-flex items-center gap-2">
          <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
          Verificando...
        </span>
        <span v-else>Verificar</span>
      </button>
    </form>
  </div>
</template>
