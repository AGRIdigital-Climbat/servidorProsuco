<script setup>
import { ref, onMounted, computed } from 'vue'
import { estructuraPermisos, asignarPermisosBatch } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'

const toast = useToastStore()
const estructura = ref(null); const loading = ref(true); const saving = ref(false); const err = ref('')
const selectedRol = ref(null)
const dirtyMap = ref({})

const roles = computed(() => estructura.value?.roles || [])
const apps = computed(() => (estructura.value?.aplicaciones || []).filter(a => !['TEST','VERIFY'].includes(a.codigo)))
const accionesFiltradas = computed(() => (estructura.value?.acciones || []).filter(a => !['TEST','VERIFY'].includes(a.codigo)))
const matriz = computed(() => estructura.value?.matriz || {})

const permisosActuales = computed(() => {
  if (!selectedRol.value) return {}
  return matriz.value[selectedRol.value] || {}
})

onMounted(async () => {
  try {
    const r = await estructuraPermisos()
    estructura.value = r.data
    if (estructura.value?.roles?.length) selectedRol.value = estructura.value.roles[0].id_rol
  } catch (e) { err.value = e.response?.data?.detail || 'Error al cargar estructura' }
  finally { loading.value = false }
})

function isChecked(appCode, accionCode) {
  if (dirtyMap.value[appCode]) return dirtyMap.value[appCode].includes(accionCode)
  const p = permisosActuales.value[appCode]
  return p && p.includes(accionCode)
}

function toggle(appCode, accionCode) {
  if (!dirtyMap.value[appCode]) dirtyMap.value[appCode] = [...(permisosActuales.value[appCode] || [])]
  const arr = dirtyMap.value[appCode]
  const idx = arr.indexOf(accionCode)
  if (idx >= 0) arr.splice(idx, 1)
  else arr.push(accionCode)
}

function selectRol(id) {
  selectedRol.value = id
  dirtyMap.value = {}
}

const dirtyCount = computed(() => {
  let n = 0
  for (const app of apps.value) { if (dirtyMap.value[app.codigo]) n++ }
  return n
})

async function guardar() {
  saving.value = true; err.value = ''
  try {
    const finalPermisos = {}
    for (const app of apps.value) {
      const code = app.codigo
      if (dirtyMap.value[code] !== undefined) {
        if (dirtyMap.value[code].length) finalPermisos[code] = dirtyMap.value[code]
        else finalPermisos[code] = []
      } else if (permisosActuales.value[code]?.length) {
        finalPermisos[code] = permisosActuales.value[code]
      }
    }
    await asignarPermisosBatch(selectedRol.value, finalPermisos)
    toast.success('Permisos actualizados')
    dirtyMap.value = {}
    const r = await estructuraPermisos()
    estructura.value = r.data
  } catch (e) { err.value = e.response?.data?.detail || 'Error al guardar' }
  finally { saving.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">✧</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">★ Matriz de Permisos</h1>
        <p class="text-sm text-slate-500">Asignar permisos por rol</p>
      </div>
    </div>

    <div v-if="loading && !estructura" class="space-y-3">
      <div class="skeleton h-10 w-96"></div>
      <div class="skeleton h-96 w-full"></div>
    </div>
    <div v-else-if="err" class="text-rose-600 bg-rose-50 border border-rose-200 p-4 rounded-xl">✕ {{ err }}</div>
    <template v-else>
      <!-- Toolbar -->
      <div class="bg-white border border-slate-200 rounded-xl p-4 mb-5 flex flex-wrap items-center gap-4 shadow-sm">
        <div class="flex items-center gap-3">
          <label class="text-xs font-medium text-slate-500 uppercase tracking-wide">Rol</label>
          <select v-model="selectedRol" @change="selectRol(selectedRol)" class="border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white">
            <option v-for="r in roles" :key="r.id_rol" :value="r.id_rol">{{ r.nombre }}</option>
          </select>
        </div>
        <div class="flex-1"></div>
        <button @click="guardar" :disabled="saving"
          class="bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
          <span v-if="saving" class="inline-flex items-center gap-2">
            <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
            Guardando...
          </span>
          <span v-else>Guardar {{ dirtyCount ? `(${dirtyCount})` : '' }}</span>
        </button>
      </div>

      <div v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-xl mb-4">✕ {{ err }}</div>

      <!-- Matrix -->
      <div class="bg-white border border-slate-200 rounded-xl overflow-x-auto shadow-sm">
        <table class="w-full text-sm">
          <thead>
            <tr class="bg-slate-50 border-b border-slate-200">
              <th class="text-left px-4 py-3 text-xs font-semibold text-slate-500 uppercase tracking-wider min-w-[140px]">Aplicación</th>
              <th v-for="a in accionesFiltradas" :key="a.codigo" class="text-center px-3 py-3 text-xs font-semibold text-slate-500 uppercase tracking-wider min-w-[72px]">{{ a.codigo }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="app in apps" :key="app.id_aplicacion" class="border-b border-slate-100 hover:bg-indigo-50/30 transition-all">
              <td class="px-4 py-3 font-medium text-slate-800 text-sm">{{ app.codigo }}
                <div class="text-xs text-slate-400 font-normal">{{ app.nombre }}</div>
              </td>
              <td v-for="a in accionesFiltradas" :key="a.codigo" class="text-center px-3 py-3">
                <label class="inline-flex items-center cursor-pointer">
                  <input type="checkbox" :checked="isChecked(app.codigo, a.codigo)"
                    @change="toggle(app.codigo, a.codigo)"
                    class="w-4 h-4 rounded border-slate-300 text-indigo-600 focus:ring-indigo-500 cursor-pointer transition-all" />
                </label>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <details class="mt-4 bg-white border border-slate-200 rounded-xl shadow-sm">
        <summary class="px-4 py-3 cursor-pointer text-sm font-semibold text-slate-500 hover:text-slate-700 transition-colors">Ver JSON</summary>
        <div class="px-4 pb-4">
          <pre class="bg-slate-50 rounded-lg p-3 text-xs overflow-x-auto border border-slate-200 font-mono">{{ JSON.stringify(permisosActuales, null, 2) }}</pre>
        </div>
      </details>
    </template>
  </div>
</template>
