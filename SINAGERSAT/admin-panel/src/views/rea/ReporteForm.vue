<script setup>
import { ref, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { crearReporte, actualizarReporte, obtenerReporte, crearReporteConFoto, listarEstados, listarCategorias } from '../../api/rea'
import { listarDepartamentos, listarProvincias, listarMunicipios } from '../../api/shared'
import { listarTiposEvento } from '../../api/edan'
import { useAuthStore } from '../../stores/auth'
import { useToastStore } from '../../stores/toast'

const router = useRouter(); const route = useRoute(); const auth = useAuthStore()
const toast = useToastStore()
const editId = ref(route.params.id || null)
const estados = ref([]); const categorias = ref([]); const departamentos = ref([])
const provincias = ref([]); const municipios = ref([]); const tiposEvento = ref([])
const file = ref(null)
const err = ref(''); const loading = ref(false)

const form = ref({
  id_usuario: auth.user?.user_id || 1, id_departamento: null, id_provincia: null, id_municipio: null,
  comunidad: '', fecha_evento: '', hora_evento: '', id_tipo_evento: null,
  descripcion: '', fotografia: '', id_estado: null, categorias: [],
})

onMounted(async () => {
  try {
    const [e, c, d, p, m, t] = await Promise.all([
      listarEstados(), listarCategorias(), listarDepartamentos(),
      listarProvincias(), listarMunicipios(), listarTiposEvento(),
    ])
    estados.value = e.data || []
    categorias.value = c.data || []
    departamentos.value = d.data || []
    provincias.value = p.data || []
    municipios.value = m.data || []
    tiposEvento.value = t.data || []
  } catch (e) { console.error(e) }
  if (editId.value) {
    try {
      const r = await obtenerReporte(editId.value)
      const d = r.data
      form.value = {
        id_usuario: d.id_usuario, id_departamento: d.id_departamento, id_provincia: d.id_provincia,
        id_municipio: d.id_municipio, comunidad: d.comunidad, fecha_evento: d.fecha_evento,
        hora_evento: d.hora_evento || '', id_tipo_evento: d.id_tipo_evento,
        descripcion: d.descripcion || '', fotografia: d.fotografia || '', id_estado: d.id_estado,
        categorias: d.categorias?.map(c => c.id_categoria) || [],
      }
    } catch (e) { err.value = 'Error al cargar reporte' }
  }
})

async function submit() {
  err.value = ''; loading.value = true
  try {
    const payload = { ...form.value }
    if (!payload.hora_evento) delete payload.hora_evento
    if (!payload.fotografia) delete payload.fotografia
    if (payload.id_departamento === null) delete payload.id_departamento
    if (payload.id_provincia === null) delete payload.id_provincia
    if (payload.id_tipo_evento === null) delete payload.id_tipo_evento
    const cats = payload.categorias?.filter(Boolean)
    payload.categorias = cats?.length ? JSON.stringify(cats) : null

    if (editId.value) {
      await actualizarReporte(editId.value, payload)
      toast.success('Reporte actualizado')
    } else if (file.value) {
      const fd = new FormData()
      fd.append('file', file.value)
      Object.entries(payload).forEach(([k, v]) => { if (v !== null && v !== undefined) fd.append(k, v) })
      if (payload.categorias) fd.append('categorias', payload.categorias)
      await crearReporteConFoto(fd)
      toast.success('Reporte creado con foto')
    } else {
      await crearReporte(payload)
      toast.success('Reporte creado')
    }
    router.push('/rea/reportes')
  } catch (e) { err.value = e.response?.data?.detail || 'Error al guardar' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">☰</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">{{ editId ? 'Editar' : 'Nuevo' }} Reporte REA</h1>
        <p class="text-sm text-slate-500">{{ editId ? 'Modificar reporte de evento' : 'Registrar un nuevo evento' }}</p>
      </div>
    </div>

    <form @submit.prevent="submit" class="max-w-2xl bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div class="grid grid-cols-3 gap-4">
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
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Municipio *</label>
          <select v-model.number="form.id_municipio" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null" disabled>Seleccionar</option>
            <option v-for="m in municipios" :key="m.id" :value="m.id">{{ m.nombre }}</option>
          </select>
        </div>
        <div class="col-span-2">
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Comunidad</label>
          <input v-model="form.comunidad" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Fecha Evento *</label>
          <input v-model="form.fecha_evento" type="date" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Hora</label>
          <input v-model="form.hora_evento" type="time" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Tipo Evento</label>
          <select v-model.number="form.id_tipo_evento" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null">Seleccionar</option>
            <option v-for="t in tiposEvento" :key="t.id_tipo_evento" :value="t.id_tipo_evento">{{ t.nombre }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Estado</label>
          <select v-model.number="form.id_estado" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null" disabled>Seleccionar</option>
            <option v-for="e in estados" :key="e.id_estado" :value="e.id_estado">{{ e.nombre }}</option>
          </select>
        </div>
      </div>
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Descripción</label>
        <textarea v-model="form.descripcion" rows="3" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
      </div>
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-2">Categorías de afectación</label>
        <div class="flex flex-wrap gap-2">
          <label v-for="c in categorias" :key="c.id_categoria"
            class="flex items-center gap-1.5 px-3 py-1.5 rounded-lg border text-sm cursor-pointer transition-all"
            :class="form.categorias.includes(c.id_categoria) ? 'border-indigo-300 bg-indigo-50 text-indigo-700' : 'border-slate-200 text-slate-600 hover:border-slate-300'">
            <input type="checkbox" :value="c.id_categoria" v-model="form.categorias" class="accent-indigo-600" />
            {{ c.nombre }}
          </label>
        </div>
      </div>
      <div v-if="!editId">
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Foto (opcional)</label>
        <input type="file" @change="file = $event.target.files[0]" accept="image/*"
          class="text-sm text-slate-600 file:mr-4 file:py-2 file:px-4 file:rounded-lg file:border-0 file:text-sm file:font-semibold file:bg-indigo-50 file:text-indigo-700 hover:file:bg-indigo-100" />
      </div>
      <p v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ err }}</p>
      <div class="flex gap-3 pt-2">
        <button type="submit" :disabled="loading"
          class="bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
          <span v-if="loading" class="inline-flex items-center gap-2">
            <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
            Guardando...
          </span>
          <span v-else>Guardar</span>
        </button>
        <button type="button" @click="router.push('/rea/reportes')" class="text-slate-600 hover:bg-slate-100 px-4 py-2 text-sm font-medium rounded-lg transition-all">Cancelar</button>
      </div>
    </form>
  </div>
</template>
