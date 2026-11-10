import api from './axios'

export function login(usuario, contrasena) {
  return api.post('/auth/login', { usuario, contrasena }).then(r => r.data)
}

export function register(data) {
  return api.post('/auth/register', data).then(r => r.data)
}

export function verificarIdentidad(data) {
  return api.post('/auth/verificar-identidad', data).then(r => r.data)
}

export function cambiarContrasena(id_usuario, nueva_contrasena) {
  return api.post('/auth/cambiar-contrasena', { id_usuario, nueva_contrasena }).then(r => r.data)
}

export function misPermisos() {
  return api.get('/auth/mis-permisos').then(r => r.data)
}
