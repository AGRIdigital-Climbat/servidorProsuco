<script setup>
import { ref, onMounted } from 'vue'
import { subirArchivo, listarArchivos } from '../../api/upload'
import { listarMunicipios } from '../../api/shared'
import { useToastStore } from '../../stores/toast'

const toast = useToastStore()
const tab = ref('subir') // 'subir' | 'galeria'
const file = ref(null); const id_municipio = ref(null); const comunidad = ref('')
const municipios = ref([])
const result = ref(null); const err = ref(''); const loading = ref(false)

const files = ref([]); const galLoading = ref(false)
const viewer = ref(null) // file being viewed in lightbox

async function submit() {
  err.value = ''; result.value = null; loading.value = true
  try {
    const fd = new FormData()
    fd.append('file', file.value)
    if (id_municipio.value) fd.append('id_municipio', id_municipio.value)
    if (comunidad.value) fd.append('comunidad', comunidad.value)
    const r = await subirArchivo(fd)
    result.value = r.data
    toast.success('Archivo subido exitosamente')
  } catch (e) { err.value = e.response?.data?.detail || 'Error al subir' }
  finally { loading.value = false }
}

async function loadGaleria() {
  galLoading.value = true
  try {
    const r = await listarArchivos()
    files.value = r.data || []
  } catch (e) { console.error(e) }
  finally { galLoading.value = false }
}

function formatDate(ts) {
  const d = new Date(ts * 1000)
  return d.toLocaleDateString() + ' ' + d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB'
  return (bytes / 1048576).toFixed(1) + ' MB'
}

onMounted(async () => {
  loadGaleria()
  try { const r = await listarMunicipios(); municipios.value = r.data || [] } catch (e) {}
})
</script>

<template>
  <div>
    <div class="flex items-center justify-between gap-4 mb-6">
      <div class="flex items-center gap-4">
        <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⇧</div>
        <div>
          <h1 class="text-xl font-bold text-slate-900">Archivos</h1>
          <p class="text-sm text-slate-500">Subir y gestionar imágenes</p>
        </div>
      </div>
      <div class="flex bg-slate-100 rounded-lg p-0.5 gap-0.5">
        <button @click="tab = 'subir'" :class="['px-4 py-1.5 rounded-md text-sm font-medium transition-all', tab === 'subir' ? 'bg-white text-indigo-700 shadow-sm' : 'text-slate-500 hover:text-slate-700']">Subir</button>
        <button @click="tab = 'galeria'; loadGaleria()" :class="['px-4 py-1.5 rounded-md text-sm font-medium transition-all', tab === 'galeria' ? 'bg-white text-indigo-700 shadow-sm' : 'text-slate-500 hover:text-slate-700']">Galería</button>
      </div>
    </div>

    <!-- Upload tab -->
    <form v-if="tab === 'subir'" @submit.prevent="submit" class="max-w-lg bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div class="border-2 border-dashed border-slate-200 rounded-xl p-8 text-center hover:border-indigo-400 transition-colors cursor-pointer group" @click="$refs.fileInput.click()">
        <div class="text-4xl text-slate-300 mb-3 group-hover:text-indigo-500 transition-colors">⇧</div>
        <p class="text-sm text-slate-500">{{ file ? file.name : 'Haz clic para seleccionar' }}</p>
        <p class="text-xs text-slate-400 mt-1">jpg, png, webp, gif — máx 5MB</p>
      </div>
      <input ref="fileInput" type="file" @change="file = $event.target.files[0]" accept="image/jpeg,image/png,image/webp,image/gif" class="hidden" />

      <div class="grid grid-cols-2 gap-4">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Municipio</label>
          <select v-model.number="id_municipio" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null">Seleccionar</option>
            <option v-for="m in municipios" :key="m.id" :value="m.id">{{ m.nombre }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Comunidad</label>
          <input v-model="comunidad" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
      </div>

      <p v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ err }}</p>
      <div v-if="result" class="text-emerald-700 bg-emerald-50 border border-emerald-200 p-3 rounded-lg text-sm break-all flex items-center gap-2">
        ✓ Subido: <a :href="result.path" target="_blank" class="underline font-medium">{{ result.path }}</a>
      </div>

      <button type="submit" :disabled="loading || !file"
        class="w-full bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2.5 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
        <span v-if="loading" class="inline-flex items-center justify-center gap-2">
          <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
          Subiendo...
        </span>
        <span v-else>Subir Archivo</span>
      </button>
    </form>

    <!-- Gallery tab -->
    <div v-else>
      <!-- Lightbox -->
      <div v-if="viewer" class="fixed inset-0 z-[9999] bg-black/80 flex items-center justify-center p-4" @click="viewer = null">
        <img :src="viewer.path" class="max-w-full max-h-full rounded-xl shadow-2xl object-contain" @click.stop />
        <button @click="viewer = null" class="absolute top-4 right-4 text-white text-2xl w-10 h-10 rounded-full bg-black/40 hover:bg-black/60 flex items-center justify-center">✕</button>
      </div>

      <div v-if="galLoading" class="flex items-center justify-center py-20 text-slate-400 text-sm">Cargando imágenes...</div>

      <template v-else-if="files.length === 0">
        <div class="text-center py-20 text-slate-400">
          <div class="text-5xl mb-4 opacity-40">∅</div>
          <p class="font-medium">No hay imágenes</p>
          <p class="text-sm">Sube imágenes desde la pestaña Subir</p>
        </div>
      </template>

      <div v-else class="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-4">
        <div v-for="f in files" :key="f.path" class="group bg-white border border-slate-200 rounded-xl overflow-hidden shadow-sm hover:shadow-lg transition-all cursor-pointer" @click="viewer = f">
          <div class="aspect-square bg-slate-100 overflow-hidden">
            <img :src="f.path" :alt="f.filename" class="w-full h-full object-cover group-hover:scale-105 transition-transform duration-300" loading="lazy" />
          </div>
          <div class="p-2.5">
            <p class="text-xs text-slate-700 truncate font-medium" :title="f.filename">{{ f.filename }}</p>
            <p class="text-[10px] text-slate-400 mt-0.5">{{ formatDate(f.modified) }} · {{ formatSize(f.size) }}</p>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>