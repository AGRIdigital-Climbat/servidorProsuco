import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import AppLayout from '../components/AppLayout.vue'
import Login from '../views/Login.vue'
import Dashboard from '../views/Dashboard.vue'

const routes = [
  { path: '/login', name: 'Login', component: Login, meta: { guest: true } },

  {
    path: '/',
    component: AppLayout,
    meta: { requiresAuth: true },
    children: [
      { path: '', name: 'Dashboard', component: Dashboard },

      // Auth
      { path: 'auth/register', name: 'Register', component: () => import('../views/auth/Register.vue') },
      { path: 'auth/verificar-identidad', name: 'VerificarIdentidad', component: () => import('../views/auth/VerificarIdentidad.vue') },
      { path: 'auth/cambiar-contrasena', name: 'CambiarContrasena', component: () => import('../views/auth/CambiarContrasena.vue') },
      { path: 'auth/mis-permisos', name: 'MisPermisos', component: () => import('../views/auth/MisPermisos.vue') },

      // Usuarios
      { path: 'usuarios', name: 'Usuarios', component: () => import('../views/usuarios/UsuariosList.vue') },
      { path: 'usuarios/nuevo', name: 'UsuarioNuevo', component: () => import('../views/usuarios/UsuarioForm.vue') },
      { path: 'usuarios/:id/editar', name: 'UsuarioEditar', component: () => import('../views/usuarios/UsuarioForm.vue'), props: true },
      { path: 'usuarios/roles', name: 'Roles', component: () => import('../views/usuarios/RolesList.vue') },
      { path: 'usuarios/roles/nuevo', name: 'RolNuevo', component: () => import('../views/usuarios/RolForm.vue') },
      { path: 'usuarios/roles/:id/editar', name: 'RolEditar', component: () => import('../views/usuarios/RolForm.vue'), props: true },
      { path: 'usuarios/aplicaciones', name: 'Aplicaciones', component: () => import('../views/usuarios/AplicacionesList.vue') },
      { path: 'usuarios/acciones', name: 'Acciones', component: () => import('../views/usuarios/AccionesList.vue') },
      { path: 'usuarios/permisos', name: 'Permisos', component: () => import('../views/usuarios/PermisosView.vue') },
      { path: 'usuarios/sesiones', name: 'Sesiones', component: () => import('../views/usuarios/SesionesList.vue') },
      { path: 'usuarios/auditoria', name: 'Auditoria', component: () => import('../views/usuarios/AuditoriaList.vue') },

      // Geografía
      { path: 'geo/departamentos', name: 'Departamentos', component: () => import('../views/geo/DepartamentosList.vue') },
      { path: 'geo/provincias', name: 'Provincias', component: () => import('../views/geo/ProvinciasList.vue') },
      { path: 'geo/municipios', name: 'Municipios', component: () => import('../views/geo/MunicipiosList.vue') },

      // REA
      { path: 'rea/reportes', name: 'Reportes', component: () => import('../views/rea/ReportesList.vue') },
      { path: 'rea/reportes/nuevo', name: 'ReporteNuevo', component: () => import('../views/rea/ReporteForm.vue') },
      { path: 'rea/reportes/:id/editar', name: 'ReporteEditar', component: () => import('../views/rea/ReporteForm.vue'), props: true },
      { path: 'rea/estados', name: 'Estados', component: () => import('../views/rea/EstadosList.vue') },
      { path: 'rea/categorias', name: 'Categorias', component: () => import('../views/rea/CategoriasList.vue') },

      // Upload
      { path: 'upload', name: 'Upload', component: () => import('../views/upload/UploadView.vue') },
    ],
  },
]

const router = createRouter({ history: createWebHistory(), routes })

router.beforeEach((to, from, next) => {
  const auth = useAuthStore()
  if (to.meta.requiresAuth && !auth.isAuthenticated) next('/login')
  else if (to.meta.guest && auth.isAuthenticated) next('/')
  else next()
})

export default router
