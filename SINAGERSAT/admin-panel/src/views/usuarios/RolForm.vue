<script setup>
import { ref, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { crearRol, actualizarRol, obtenerRol } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'

const router = useRouter(); const route = useRoute()
const toast = useToastStore()
const editId = ref(route.params.id || null)
const form = ref({ nombre: '', descripcion: '' })
const err = ref(''); const loading = ref(false)

onMounted(async () => {
  if (editId.value) {
    try {
      const r = await obtenerRol(editId.value)
      form.value = { nombre: r.data.nombre, descripcion: r.data.descripcion || '' }
    } catch (e) { err.value = 'Error al cargar rol' }
  }
})

async function submit() {
  err.value = ''; loading.value = true
  try {
    if (editId.value) { await actualizarRol(editId.value, form.value); toast.success('Rol actualizado') }
    else { await crearRol(form.value); toast.success('Rol creado') }
    router.push('/usuarios/roles')
  } catch (e) { err.value = e.response?.data?.detail || 'Error al guardar' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚿</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">{{ editId ? 'Editar' : 'Nuevo' }} Rol</h1>
        <p class="text-sm text-slate-500">{{ editId ? 'Modificar datos del rol' : 'Crear un nuevo rol' }}</p>
      </div>
    </div>
    <form @submit.prevent="submit" class="max-w-md bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nombre *</label>
        <input v-model="form.nombre" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
      </div>
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Descripción</label>
        <textarea v-model="form.descripcion" rows="3" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
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
        <button type="button" @click="router.push('/usuarios/roles')" class="text-slate-600 hover:bg-slate-100 px-4 py-2 text-sm font-medium rounded-lg transition-all">Cancelar</button>
      </div>
    </form>
  </div>
</template>
