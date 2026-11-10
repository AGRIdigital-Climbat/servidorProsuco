import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { login as apiLogin } from '../api/auth'

export const useAuthStore = defineStore('auth', () => {
  const token = ref(localStorage.getItem('token') || '')
  const user = ref(JSON.parse(localStorage.getItem('user') || 'null'))

  const isAuthenticated = computed(() => !!token.value)
  const isSuperadmin = computed(() => user.value?.id_rol === 1)
  const permisosAgrupados = computed(() => user.value?.permisos || {})

  function tienePermiso(app, accion) {
    if (isSuperadmin.value) return true
    const acciones = permisosAgrupados.value[app]
    return acciones && acciones.includes(accion)
  }

  async function login(usuario, contrasena) {
    const res = await apiLogin(usuario, contrasena)
    const d = res.data || res
    token.value = d.access_token
    user.value = d
    localStorage.setItem('token', d.access_token)
    localStorage.setItem('user', JSON.stringify(d))
    return d
  }

  function logout() {
    token.value = ''
    user.value = null
    localStorage.removeItem('token')
    localStorage.removeItem('user')
  }

  return { token, user, isAuthenticated, isSuperadmin, permisosAgrupados, tienePermiso, login, logout }
})
