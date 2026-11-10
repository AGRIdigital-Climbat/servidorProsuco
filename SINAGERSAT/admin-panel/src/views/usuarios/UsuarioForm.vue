<script setup>
import { ref, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { crearUsuario, actualizarUsuario, obtenerUsuario } from '../../api/usuarios'
import { listarRoles } from '../../api/usuarios'
import { useToastStore } from '../../stores/toast'

const router = useRouter(); const route = useRoute()
const toast = useToastStore()
const editId = ref(route.params.id || null)
const roles = ref([])
const form = ref({ usuario: '', contrasena: '', nombre: '', celular: '', carnet: '', correo: '', cargo: '', id_rol: null, activo: true })
const err = ref(''); const loading = ref(false)

onMounted(async () => {
  try { const r = await listarRoles(); roles.value = r.data || [] } catch (e) { console.error(e) }
  if (editId.value) {
    try {
      const r = await obtenerUsuario(editId.value)
      const u = r.data
      form.value = { usuario: u.usuario, contrasena: '', nombre: u.nombre, celular: u.celular || '', carnet: u.carnet || '', correo: u.correo || '', cargo: u.cargo || '', id_rol: u.id_rol, activo: u.activo }
    } catch (e) { err.value = 'Error al cargar usuario' }
  }
})

async function submit() {
  err.value = ''; loading.value = true
  try {
    if (editId.value) {
      const payload = { ...form.value }
      if (!payload.contrasena) delete payload.contrasena
      await actualizarUsuario(editId.value, payload)
      toast.success('Usuario actualizado')
    } else {
      await crearUsuario(form.value)
      toast.success('Usuario creado')
    }
    router.push('/usuarios')
  } catch (e) { err.value = e.response?.data?.detail || 'Error al guardar' }
  finally { loading.value = false }
}
</script>

<template>
  <div>
    <div class="flex items-center gap-4 mb-6">
      <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg">⚇</div>
      <div>
        <h1 class="text-xl font-bold text-slate-900">{{ editId ? 'Editar' : 'Nuevo' }} Usuario</h1>
        <p class="text-sm text-slate-500">{{ editId ? 'Modificar datos del usuario' : 'Crear un nuevo usuario en el sistema' }}</p>
      </div>
    </div>

    <form @submit.prevent="submit" class="max-w-2xl bg-white border border-slate-200 rounded-xl p-6 shadow-sm space-y-4">
      <div class="grid grid-cols-2 gap-4">
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">Usuario *</label>
          <input v-model="form.usuario" required class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 uppercase tracking-wide mb-1">{{ editId ? 'Nueva contraseña' : 'Contraseña *' }}</label>
          <input v-model="form.contrasena" :required="!editId" type="password" class="w-full border border-slate-200 rounded-lg px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-slate-50/50" />
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
      <div v-if="editId" class="flex items-center gap-3">
        <label class="text-xs font-medium text-slate-500 uppercase tracking-wide">Activo:</label>
        <label class="relative inline-flex items-center cursor-pointer">
          <input v-model="form.activo" type="checkbox" class="sr-only peer" />
          <div class="w-9 h-5 bg-slate-200 rounded-full peer peer-checked:bg-emerald-500 peer-checked:after:translate-x-full after:content-[''] after:absolute after:top-0.5 after:left-[2px] after:bg-white after:rounded-full after:h-4 after:w-4 after:transition-all"></div>
        </label>
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
        <button type="button" @click="router.push('/usuarios')" class="text-slate-600 hover:bg-slate-100 px-4 py-2 text-sm font-medium rounded-lg transition-all">Cancelar</button>
      </div>
    </form>
  </div>
</template>
