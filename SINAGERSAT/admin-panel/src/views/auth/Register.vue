<script setup>
import { ref, onMounted } from 'vue'
import { register } from '../../api/auth'
import { listarRoles } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'

const toast = useToastStore()
const form = ref({ usuario: '', contrasena: '', nombre: '', celular: '', carnet: '', correo: '', cargo: '', id_rol: null })
const roles = ref([])
const err = ref(''); const loading = ref(false)

onMounted(async () => {
  try { const r = await listarRoles(); roles.value = r.data || [] } catch (e) { console.error(e) }
})

async function submit() {
  err.value = ''; loading.value = true
  try {
    const r = await register(form.value)
    toast.success(`Usuario creado ID: ${r.id}`)
    form.value = { usuario: '', contrasena: '', nombre: '', celular: '', carnet: '', correo: '', cargo: '', id_rol: null }
  } catch (e) { err.value = e.response?.data?.detail || 'Error al registrar' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚿</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">Registrar Usuario</h1>
        <p class="text-sm text-slate-500">Crear una nueva cuenta en el sistema</p>
      </div>
    </div>

    <form @submit.prevent="submit" class="max-w-2xl bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div class="grid grid-cols-2 gap-4">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Usuario *</label>
          <input v-model="form.usuario" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Contraseña *</label>
          <input v-model="form.contrasena" type="password" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Nombre *</label>
          <input v-model="form.nombre" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Celular</label>
          <input v-model="form.celular" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Carnet</label>
          <input v-model="form.carnet" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Correo</label>
          <input v-model="form.correo" type="email" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Cargo</label>
          <input v-model="form.cargo" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Rol *</label>
          <select v-model.number="form.id_rol" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50">
            <option :value="null" disabled>Seleccionar</option>
            <option v-for="r in roles" :key="r.id_rol" :value="r.id_rol">{{ r.nombre }}</option>
          </select>
        </div>
      </div>
      <p v-if="err" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ err }}</p>
      <div class="flex gap-3 pt-2">
        <button type="submit" :disabled="loading"
          class="bg-indigo-600 hover:bg-indigo-700 text-white px-5 py-2 rounded-lg text-sm font-medium transition-all duration-200 disabled:opacity-50 shadow-sm">
          <span v-if="loading" class="inline-flex items-center gap-2">
            <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
            Registrando...
          </span>
          <span v-else>Registrar</span>
        </button>
      </div>
    </form>
  </div>
</template>
