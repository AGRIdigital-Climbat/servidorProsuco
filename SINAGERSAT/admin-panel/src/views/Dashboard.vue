<script setup>
import { useAuthStore } from '../stores/auth'
import { useRouter } from 'vue-router'
const auth = useAuthStore()
const router = useRouter()

const stats = [
  { label: 'Usuarios', value: '—', icon: '⚇', path: '/usuarios', color: 'indigo' },
  { label: 'Roles', value: '7', icon: '⚿', path: '/usuarios/roles', color: 'emerald' },
  { label: 'Municipios', value: '340', icon: '⌗', path: '/geo/municipios', color: 'amber' },
  { label: 'Reportes REA', value: '—', icon: '☰', path: '/rea/reportes', color: 'rose' },
]

const colorMap = {
  indigo: { bg: 'bg-indigo-50', text: 'text-indigo-600', bar: 'bg-indigo-500', iconBg: 'bg-indigo-100', iconText: 'text-indigo-600' },
  emerald: { bg: 'bg-emerald-50', text: 'text-emerald-600', bar: 'bg-emerald-500', iconBg: 'bg-emerald-100', iconText: 'text-emerald-600' },
  amber: { bg: 'bg-amber-50', text: 'text-amber-600', bar: 'bg-amber-500', iconBg: 'bg-amber-100', iconText: 'text-amber-600' },
  rose: { bg: 'bg-rose-50', text: 'text-rose-600', bar: 'bg-rose-500', iconBg: 'bg-rose-100', iconText: 'text-rose-600' },
}

const quickActions = [
  { label: 'Nuevo Usuario', icon: '⚇', path: '/usuarios/nuevo', desc: 'Registrar un nuevo usuario en el sistema' },
  { label: '★ Matriz Permisos', icon: '✧', path: '/usuarios/permisos', desc: 'Asignar permisos por rol' },
  { label: 'Nuevo Reporte', icon: '☰', path: '/rea/reportes/nuevo', desc: 'Registrar un evento REA' },
  { label: 'Subir Archivo', icon: '⇧', path: '/upload', desc: 'Subir imagen al servidor' },
  { label: 'Auditoría', icon: '⊞', path: '/usuarios/auditoria', desc: 'Ver registro de actividades' },
  { label: 'Verificar Identidad', icon: '⚿', path: '/auth/verificar-identidad', desc: 'Validar datos de usuario' },
]
</script>

<template>
  <div>
    <div class="mb-8">
      <div class="flex items-center gap-3">
        <div class="w-10 h-10 rounded-xl bg-indigo-100 text-indigo-700 flex items-center justify-center text-lg font-bold">{{ (auth.user?.nombre || auth.user?.usuario || '?')[0] }}</div>
        <div>
          <h1 class="text-2xl font-bold text-slate-900">Bienvenido</h1>
          <p class="text-sm text-slate-500">{{ auth.user?.nombre || auth.user?.usuario }}</p>
        </div>
      </div>
    </div>

    <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
      <div v-for="s in stats" :key="s.label" @click="router.push(s.path)"
        class="bg-white border border-slate-200 rounded-xl cursor-pointer hover:shadow-lg hover:-translate-y-0.5 transition-all duration-200 group overflow-hidden">
        <div :class="['h-1', colorMap[s.color].bar]"></div>
        <div class="p-5">
          <div class="flex items-center justify-between mb-4">
            <div :class="['w-11 h-11 rounded-xl flex items-center justify-center text-lg', colorMap[s.color].iconBg, colorMap[s.color].iconText]">{{ s.icon }}</div>
            <span :class="['text-xs font-medium', colorMap[s.color].text]">Ver →</span>
          </div>
          <div class="text-2xl font-bold text-slate-800 mb-1">{{ s.value }}</div>
          <div class="text-sm text-slate-500">{{ s.label }}</div>
        </div>
      </div>
    </div>

    <h2 class="text-lg font-bold text-slate-800 mb-4">Acciones Rápidas</h2>
    <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
      <div v-for="(a, i) in quickActions" :key="a.label" @click="router.push(a.path)"
        class="bg-white border border-slate-200 rounded-xl p-4 cursor-pointer hover:border-indigo-300 hover:shadow-md hover:-translate-y-0.5 transition-all duration-200 group">
        <div class="flex items-center gap-3">
          <div :class="['w-10 h-10 rounded-lg flex items-center justify-center text-lg transition-colors', i % 3 === 0 ? 'bg-indigo-50 text-indigo-600 group-hover:bg-indigo-100' : i % 3 === 1 ? 'bg-emerald-50 text-emerald-600 group-hover:bg-emerald-100' : 'bg-amber-50 text-amber-600 group-hover:bg-amber-100']">{{ a.icon }}</div>
          <div class="flex-1 min-w-0">
            <div class="text-sm font-semibold text-slate-800">{{ a.label }}</div>
            <div class="text-xs text-slate-400 truncate">{{ a.desc }}</div>
          </div>
          <span class="text-slate-300 group-hover:text-indigo-500 transition-colors">→</span>
        </div>
      </div>
    </div>
  </div>
</template>