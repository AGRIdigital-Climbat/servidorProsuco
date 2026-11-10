<script setup>
import { ref } from 'vue'
import { useAuthStore } from '../stores/auth'
import { useRouter } from 'vue-router'

const auth = useAuthStore()
const router = useRouter()
const usuario = ref('')
const contrasena = ref('')
const error = ref('')
const loading = ref(false)

async function handleLogin() {
  error.value = ''
  loading.value = true
  try {
    await auth.login(usuario.value, contrasena.value)
    router.push('/')
  } catch (e) {
    error.value = e.response?.data?.detail || e.response?.data?.message || 'Error al iniciar sesión'
  } finally {
    loading.value = false
  }
}
</script>

<template>
  <div class="min-h-screen flex items-center justify-center bg-gradient-to-br from-slate-900 via-indigo-950 to-slate-900">
    <div class="bg-white rounded-2xl shadow-2xl w-full max-w-sm mx-4 p-8 animate-[fadeIn_0.4s_ease-out]">
      <div class="text-center mb-8">
        <div class="text-4xl mb-2">▦</div>
        <h1 class="text-2xl font-bold text-slate-900">SINAGERSAT</h1>
        <p class="text-sm text-slate-500 mt-1">Panel de Administración</p>
      </div>
      <form @submit.prevent="handleLogin" class="space-y-4">
        <div>
          <label class="block text-xs font-medium text-slate-500 mb-1.5 uppercase tracking-wide">Usuario</label>
          <input v-model="usuario" type="text" required autocomplete="username"
            class="w-full px-3 py-2.5 border border-slate-200 rounded-lg focus:ring-2 focus:ring-indigo-500 focus:border-indigo-500 outline-none bg-slate-50/50 transition-all text-sm" />
        </div>
        <div>
          <label class="block text-xs font-medium text-slate-500 mb-1.5 uppercase tracking-wide">Contraseña</label>
          <input v-model="contrasena" type="password" required autocomplete="current-password"
            class="w-full px-3 py-2.5 border border-slate-200 rounded-lg focus:ring-2 focus:ring-indigo-500 focus:border-indigo-500 outline-none bg-slate-50/50 transition-all text-sm" />
        </div>
        <p v-if="error" class="text-rose-600 text-sm bg-rose-50 border border-rose-200 p-3 rounded-lg flex items-center gap-2">✕ {{ error }}</p>
        <button type="submit" :disabled="loading"
          class="w-full bg-indigo-600 hover:bg-indigo-700 text-white font-semibold py-2.5 rounded-lg transition-all duration-200 disabled:opacity-50 shadow-lg shadow-indigo-600/20">
          <span v-if="loading" class="inline-flex items-center justify-center gap-2">
            <svg class="animate-spin h-4 w-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"/><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z"/></svg>
            Ingresando...
          </span>
          <span v-else>Ingresar</span>
        </button>
      </form>
    </div>
  </div>
</template>

<style>
@keyframes fadeIn {
  from { opacity: 0; transform: translateY(10px); }
  to { opacity: 1; transform: translateY(0); }
}
</style>
