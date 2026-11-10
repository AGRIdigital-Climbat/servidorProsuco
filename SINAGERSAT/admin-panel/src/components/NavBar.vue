<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useAuthStore } from '../stores/auth'

const auth = useAuthStore()
const router = useRouter()
const route = useRoute()
const openDropdown = ref(null)
const showUserMenu = ref(false)

function handleClick(e) {
  const dd = e.target.closest('[data-dropdown]')
  if (!dd && openDropdown.value !== null) {
    openDropdown.value = null
    showUserMenu.value = false
  }
}
onMounted(() => document.addEventListener('click', handleClick))
onUnmounted(() => document.removeEventListener('click', handleClick))

const initials = computed(() => {
  const name = auth.user?.nombre || auth.user?.usuario || '?'
  return name.split(' ').map(w => w[0]).join('').toUpperCase().slice(0, 2)
})

const menu = [
  { label: 'Dashboard', path: '/', icon: '⊞' },
  {
    label: 'Auth', icon: '⚿', children: [
      { label: 'Register', path: '/auth/register' },
      { label: 'Verificar Identidad', path: '/auth/verificar-identidad' },
      { label: 'Cambiar Contraseña', path: '/auth/cambiar-contrasena' },
      { label: 'Mis Permisos', path: '/auth/mis-permisos' },
    ]
  },
  {
    label: 'Usuarios', icon: '⚇', children: [
      { label: 'Usuarios', path: '/usuarios' },
      { label: 'Roles', path: '/usuarios/roles' },
      { label: 'Aplicaciones', path: '/usuarios/aplicaciones' },
      { label: 'Acciones', path: '/usuarios/acciones' },
      { label: '★ Permisos', path: '/usuarios/permisos' },
      { label: 'Sesiones', path: '/usuarios/sesiones' },
      { label: 'Auditoría', path: '/usuarios/auditoria' },
    ]
  },
  {
    label: 'Geografía', icon: '⌗', children: [
      { label: 'Departamentos', path: '/geo/departamentos' },
      { label: 'Provincias', path: '/geo/provincias' },
      { label: 'Municipios', path: '/geo/municipios' },
    ]
  },
  {
    label: 'REA', icon: '☰', children: [
      { label: 'Reportes', path: '/rea/reportes' },
      { label: 'Estados', path: '/rea/estados' },
      { label: 'Categorías', path: '/rea/categorias' },
    ]
  },
  { label: 'Upload', path: '/upload', icon: '⇧' },
]

function isActive(item) {
  if (item.path === '/') return route.path === '/'
  if (item.path) return route.path.startsWith(item.path)
  return false
}

function isChildActive(children) {
  return children?.some(c => route.path.startsWith(c.path))
}

function closeDropdowns() {
  openDropdown.value = null
  showUserMenu.value = false
}

function go(path) {
  closeDropdowns()
  router.push(path)
}

function logout() {
  closeDropdowns()
  auth.logout()
  router.push('/login')
}
</script>

<template>
  <header class="bg-white border-b border-slate-200 shadow-sm sticky top-0 z-[1000]">
    <div class="flex items-center justify-between h-14 px-6">
      <!-- Logo -->
      <div class="flex items-center gap-3 shrink-0">
        <span class="text-lg">▦</span>
        <span class="font-bold text-slate-800 text-lg tracking-tight hidden sm:block">SINAGERSAT</span>
      </div>

      <!-- Desktop navigation -->
      <nav class="hidden md:flex items-center gap-1 mx-6">
        <template v-for="item in menu" :key="item.label">
          <!-- Single link -->
          <button v-if="item.path" @click="go(item.path)"
            :class="['px-3 py-1.5 rounded-lg text-sm font-medium transition-all duration-150', isActive(item) ? 'text-indigo-700 bg-indigo-50' : 'text-slate-600 hover:text-slate-900 hover:bg-slate-50']">
            {{ item.label }}
          </button>
          <!-- Dropdown -->
          <div v-else data-dropdown class="relative">
            <button @click="openDropdown = openDropdown === item.label ? null : item.label"
              :class="['flex items-center gap-1 px-3 py-1.5 rounded-lg text-sm font-medium transition-all duration-150', isChildActive(item.children) ? 'text-indigo-700 bg-indigo-50' : 'text-slate-600 hover:text-slate-900 hover:bg-slate-50']">
              {{ item.label }}
              <span class="text-xs opacity-60">▾</span>
            </button>
            <div v-show="openDropdown === item.label"
              class="absolute top-full left-0 mt-1 w-52 bg-white border border-slate-200 rounded-xl shadow-xl py-1.5 overflow-hidden z-50">
              <button v-for="ch in item.children" :key="ch.path" @click="go(ch.path)"
                :class="['w-full text-left px-4 py-2 text-sm transition-all', route.path.startsWith(ch.path) ? 'text-indigo-700 bg-indigo-50 font-medium' : 'text-slate-600 hover:bg-slate-50 hover:text-slate-900']">
                {{ ch.label }}
              </button>
            </div>
          </div>
        </template>
      </nav>

      <!-- Mobile menu toggle -->
      <div class="md:hidden flex-1"></div>

      <!-- User -->
      <div data-dropdown class="relative shrink-0">
        <button @click="showUserMenu = !showUserMenu"
          class="flex items-center gap-2.5 px-2 py-1.5 rounded-lg hover:bg-slate-50 transition-all">
          <div class="w-7 h-7 rounded-full bg-indigo-600 text-white flex items-center justify-center text-xs font-bold">{{ initials }}</div>
          <span class="hidden sm:block text-sm font-medium text-slate-700">{{ auth.user?.nombre || auth.user?.usuario }}</span>
          <span class="text-xs text-slate-400">▾</span>
        </button>
        <div v-show="showUserMenu"
          class="absolute right-0 top-full mt-1 w-48 bg-white border border-slate-200 rounded-xl shadow-xl py-1.5 overflow-hidden z-50">
          <div class="px-4 py-2 text-xs text-slate-400 border-b border-slate-100">Rol {{ auth.user?.id_rol }}</div>
          <button @click="go('/auth/mis-permisos')" class="w-full text-left px-4 py-2 text-sm text-slate-600 hover:bg-slate-50">Mis Permisos</button>
          <button @click="logout" class="w-full text-left px-4 py-2 text-sm text-rose-600 hover:bg-rose-50 font-medium">Cerrar Sesión</button>
        </div>
      </div>
    </div>

    <!-- Mobile menu -->
    <Transition name="slide">
      <div v-if="false" class="md:hidden border-t border-slate-100 bg-white p-4 space-y-1">
        <!-- Hidden by default, could be toggled with hamburger -->
      </div>
    </Transition>
  </header>
</template>
