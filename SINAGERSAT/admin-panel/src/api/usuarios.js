import api from './axios'

export function listarUsuarios(params) { return api.get('/usuarios/', { params }).then(r => r.data) }
export function obtenerUsuario(id) { return api.get(`/usuarios/${id}`).then(r => r.data) }
export function crearUsuario(data) { return api.post('/usuarios/', data).then(r => r.data) }
export function actualizarUsuario(id, data) { return api.put(`/usuarios/${id}`, data).then(r => r.data) }
export function eliminarUsuario(id) { return api.delete(`/usuarios/${id}`).then(r => r.data) }

export function listarRoles() { return api.get('/usuarios/roles').then(r => r.data) }
export function obtenerRol(id) { return api.get(`/usuarios/roles/${id}`).then(r => r.data) }
export function crearRol(data) { return api.post('/usuarios/roles', data).then(r => r.data) }
export function actualizarRol(id, data) { return api.put(`/usuarios/roles/${id}`, data).then(r => r.data) }
export function eliminarRol(id) { return api.delete(`/usuarios/roles/${id}`).then(r => r.data) }

export function listarAplicaciones() { return api.get('/usuarios/aplicaciones').then(r => r.data) }
export function obtenerAplicacion(id) { return api.get(`/usuarios/aplicaciones/${id}`).then(r => r.data) }
export function crearAplicacion(data) { return api.post('/usuarios/aplicaciones', data).then(r => r.data) }
export function actualizarAplicacion(id, data) { return api.put(`/usuarios/aplicaciones/${id}`, data).then(r => r.data) }
export function eliminarAplicacion(id) { return api.delete(`/usuarios/aplicaciones/${id}`).then(r => r.data) }

export function listarAcciones() { return api.get('/usuarios/acciones').then(r => r.data) }
export function obtenerAccion(id) { return api.get(`/usuarios/acciones/${id}`).then(r => r.data) }
export function crearAccion(data) { return api.post('/usuarios/acciones', data).then(r => r.data) }
export function actualizarAccion(id, data) { return api.put(`/usuarios/acciones/${id}`, data).then(r => r.data) }
export function eliminarAccion(id) { return api.delete(`/usuarios/acciones/${id}`).then(r => r.data) }

export function estructuraPermisos() { return api.get('/usuarios/permisos/estructura').then(r => r.data) }
export function permisosPorRol(id) { return api.get(`/usuarios/permisos/rol/${id}`).then(r => r.data) }
export function asignarPermisosBatch(id, permisos) { return api.put(`/usuarios/roles/${id}/permisos`, { permisos }).then(r => r.data) }
export function listarPermisos(params) { return api.get('/usuarios/permisos', { params }).then(r => r.data) }
export function asignarPermiso(rol, app, accion) { return api.post(`/usuarios/permisos/${rol}/${app}/${accion}`).then(r => r.data) }
export function quitarPermiso(rol, app, accion) { return api.delete(`/usuarios/permisos/${rol}/${app}/${accion}`).then(r => r.data) }

export function listarSesiones(params) { return api.get('/usuarios/sesiones', { params }).then(r => r.data) }
export function listarAuditoria(params) { return api.get('/usuarios/auditoria', { params }).then(r => r.data) }
