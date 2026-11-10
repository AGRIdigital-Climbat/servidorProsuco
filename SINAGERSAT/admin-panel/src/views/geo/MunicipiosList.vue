<script setup>
import { ref, onMounted, onUnmounted, nextTick } from 'vue'
import { listarMunicipios, crearMunicipio, actualizarMunicipio, eliminarMunicipio, listarProvincias, listarMunicipiosGeojson } from '../../api/shared'
import { useToastStore } from '../../stores/toast'
import DataTable from '../../components/DataTable.vue'
import L from 'leaflet'
import 'leaflet/dist/leaflet.css'

const toast = useToastStore()
const data = ref([]); const provincias = ref([]); const loading = ref(true)
const showForm = ref(false); const editItem = ref(null); const form = ref({ nombre: '', codigo_ine: '', id_provincia: null })
const err = ref('')
const filtroProvincia = ref('')
const showMap = ref(false); const mapLoading = ref(false); let mapInstance = null; let geoLayer = null
const columns = [
  { key: 'id', label: 'ID' },
  { key: 'codigo_ine', label: 'Código INE' },
  { key: 'nombre', label: 'Nombre' },
  { key: 'id_provincia', label: 'Provincia' },
]

function getFilterParams() {
  const params = {}
  if (filtroProvincia.value) params.id_provincia = filtroProvincia.value
  return params
}

async function load() {
  loading.value = true
  try {
    const params = getFilterParams()
    const [r, p] = await Promise.all([listarMunicipios(params), listarProvincias()])
    data.value = r.data || []
    provincias.value = p.data || []
    if (showMap.value) await loadMapData()
  } catch (e) { console.error(e) }
  finally { loading.value = false }
}
function openCreate() { editItem.value = null; form.value = { nombre: '', codigo_ine: '', id_provincia: null }; showForm.value = true }
function openEdit(row) { editItem.value = row; form.value = { nombre: row.nombre, codigo_ine: row.codigo_ine || '', id_provincia: row.id_provincia }; showForm.value = true }
async function save() {
  err.value = ''
  try {
    if (editItem.value) { await actualizarMunicipio(editItem.value.id, form.value); toast.success('Municipio actualizado') }
    else { await crearMunicipio(form.value); toast.success('Municipio creado') }
    showForm.value = false; load()
  } catch (e) { err.value = e.response?.data?.detail || 'Error' }
}
async function del(row) { try { await eliminarMunicipio(row.id); toast.success('Municipio eliminado'); load() } catch (e) { toast.error(e.response?.data?.detail || 'Error') } }

async function toggleMap() {
  showMap.value = !showMap.value
  if (showMap.value) {
    await nextTick()
    initMap()
    await loadMapData()
  } else {
    destroyMap()
  }
}

function initMap() {
  const el = document.getElementById('municipios-map')
  if (!el || mapInstance) return
  mapInstance = L.map(el).setView([-16.5, -68.15], 7)
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a>',
    maxZoom: 18,
  }).addTo(mapInstance)
}

async function loadMapData() {
  if (!mapInstance) return
  if (geoLayer) { mapInstance.removeLayer(geoLayer); geoLayer = null }
  mapLoading.value = true
  try {
    const geojson = await listarMunicipiosGeojson(getFilterParams())
    if (geojson && geojson.features && geojson.features.length) {
      geoLayer = L.geoJSON(geojson, {
        style: { color: '#4f46e5', weight: 1, fillOpacity: 0.15 },
        onEachFeature: (feature, layer) => {
          const p = feature.properties || {}
          layer.bindPopup(`<b>${p.nombre || ''}</b><br/>INE: ${p.codigo_ine || ''}`)
        }
      }).addTo(mapInstance)
      mapInstance.fitBounds(geoLayer.getBounds())
    }
  } catch (e) { console.error(e) }
  finally { mapLoading.value = false }
}

function destroyMap() {
  if (mapInstance) { mapInstance.remove(); mapInstance = null }
}

onUnmounted(destroyMap)
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
            <label class="block text-xs text-slate-500 mb-1">Provincia</label>
            <select v-model="filtroProvincia" @change="load" class="w-full border border-slate-200 rounded-lg px-2.5 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option value="">Todas</option>
              <option v-for="p in provincias" :key="p.id" :value="p.id">{{ p.nombre }}</option>
            </select>
          </div>
          <button @click="load" class="w-full bg-indigo-600 hover:bg-indigo-700 text-white text-sm py-2 rounded-lg transition-all">Aplicar</button>
        </div>
      </div>
    </aside>

    <div class="flex-1 min-w-0">
      <div class="flex items-center justify-between mb-5">
        <div>
          <h1 class="text-xl font-bold text-slate-900">Municipios</h1>
          <p class="text-sm text-slate-500">340 municipios de Bolivia</p>
        </div>
        <div class="flex items-center gap-2">
          <button @click="toggleMap"
            class="border border-slate-200 hover:bg-slate-50 text-slate-600 px-4 py-2 rounded-lg text-sm font-medium transition-all flex items-center gap-2">
            <span>{{ showMap ? '⊞' : '⌗' }}</span>
            <span>{{ showMap ? 'Tabla' : 'Mapa' }}</span>
          </button>
          <button @click="openCreate" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm transition-all">+ Nuevo</button>
        </div>
      </div>

      <div class="lg:hidden flex gap-2 mb-4">
        <select v-model="filtroProvincia" @change="load" class="flex-1 border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white">
          <option value="">Todas las provincias</option>
          <option v-for="p in provincias" :key="p.id" :value="p.id">{{ p.nombre }}</option>
        </select>
      </div>

      <div v-if="showForm" class="bg-white border border-slate-200 rounded-xl p-5 mb-5 max-w-md shadow-sm">
        <h2 class="font-semibold text-slate-800 mb-4">{{ editItem ? 'Editar' : 'Nuevo' }} Municipio</h2>
        <div class="space-y-3">
          <div>
            <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Provincia</label>
            <select v-model.number="form.id_provincia" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
              <option :value="null" disabled>Seleccionar</option>
              <option v-for="p in provincias" :key="p.id" :value="p.id">{{ p.nombre }}</option>
            </select>
          </div>
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
            <button @click="save" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded-lg text-sm font-medium shadow-sm">Guardar</button>
            <button @click="showForm = false" class="text-slate-600 hover:bg-slate-100 px-4 py-2 rounded-lg text-sm font-medium transition-all">Cancelar</button>
          </div>
        </div>
      </div>

      <div v-if="showMap" class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden" style="height:600px">
        <div v-if="mapLoading" class="flex items-center justify-center h-full text-slate-400 text-sm">Cargando mapa...</div>
        <div id="municipios-map" class="h-full w-full"></div>
      </div>
      <div v-else class="bg-white border border-slate-200 rounded-xl shadow-sm overflow-hidden">
        <DataTable :columns="columns" :data="data" :loading="loading" @delete="del">
          <template #cell-id_provincia="{ value }">
            <span class="bg-slate-100 text-slate-600 text-xs font-medium px-2.5 py-0.5 rounded-full">{{ provincias.find(p => p.id === value)?.nombre || value }}</span>
          </template>
          <template #actions="{ row }">
            <button @click="openEdit(row)" class="p-1.5 text-slate-400 hover:text-indigo-600 hover:bg-indigo-50 rounded-lg transition-all" title="Editar">✎</button>
          </template>
        </DataTable>
      </div>
    </div>
  </div>
</template>
