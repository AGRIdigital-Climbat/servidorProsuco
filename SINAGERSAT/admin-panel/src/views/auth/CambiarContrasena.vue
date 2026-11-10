<script setup>
import { ref, onMounted } from 'vue'
import { cambiarContrasena } from '../../api/auth'
import { listarUsuarios } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'

const toast = useToastStore()
const id_usuario = ref(null); const nueva_contrasena = ref('')
const usuarios = ref([])
const err = ref(''); const loading = ref(false)

onMounted(async () => {
  try { const r = await listarUsuarios(); usuarios.value = r.data || [] } catch (e) {}
})

async function submit() {
  err.value = ''; loading.value = true
  try {
    const r = await cambiarContrasena(id_usuario.value, nueva_contrasena.value)
    toast.success(r.message || 'Contraseña cambiada exitosamente')
    id_usuario.value = null; nueva_contrasena.value = ''
  } catch (e) { err.value = e.response?.data?.detail || 'Error al cambiar contraseña' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚿</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">Cambiar Contraseña</h1>
        <p class="text-sm text-slate-500">Actualizar la contraseña de un usuario</p>
      </div>
    </div>

    <form @submit.prevent="submit" class="max-w-md bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Usuario *</label>
        <select v-model.number="id_usuario" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
          <option :value="null" disabled>Seleccionar</option>
          <option v-for="u in usuarios" :key="u.id_usuario" :value="u.id_usuario">{{ u.nombre || u.usuario }} ({{ u.usuario }})</option>
        </select>
      </div>
      <div>
        <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nueva Contraseña *</label>
        <input v-model="nueva_contrasena" type="password" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
      </div>
      <p v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ err }}</p>
      <button type="submit" :disabled="loading"
        class="bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
        <span v-if="loading" class="inline-flex items-center gap-2">
          <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
          Enviando...
        </span>
        <span v-else>Cambiar Contraseña</span>
      </button>
    </form>
  </div>
</template>
