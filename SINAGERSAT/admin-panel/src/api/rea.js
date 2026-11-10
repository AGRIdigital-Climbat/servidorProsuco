import api from './axios'

export function listarReportes(params) { return api.get('/rea/reportes', { params }).then(r => r.data) }
export function obtenerReporte(id) { return api.get(`/rea/reportes/${id}`).then(r => r.data) }
export function crearReporte(data) { return api.post('/rea/reportes', data).then(r => r.data) }
export function actualizarReporte(id, data) { return api.put(`/rea/reportes/${id}`, data).then(r => r.data) }
export function eliminarReporte(id) { return api.delete(`/rea/reportes/${id}`).then(r => r.data) }
export function cambiarEstado(id, id_estado) { return api.put(`/rea/reportes/${id}/estado`, { id_estado }).then(r => r.data) }

export function crearReporteConFoto(formData) { return api.post('/rea/reportes/con-foto', formData, { headers: { 'Content-Type': 'multipart/form-data' } }).then(r => r.data) }

export function listarEstados() { return api.get('/rea/estados').then(r => r.data) }
export function crearEstado(data) { return api.post('/rea/estados', data).then(r => r.data) }
export function actualizarEstado(id, data) { return api.put(`/rea/estados/${id}`, data).then(r => r.data) }
export function eliminarEstado(id) { return api.delete(`/rea/estados/${id}`).then(r => r.data) }

export function listarCategorias() { return api.get('/rea/categorias').then(r => r.data) }
export function crearCategoria(data) { return api.post('/rea/categorias', data).then(r => r.data) }
export function actualizarCategoria(id, data) { return api.put(`/rea/categorias/${id}`, data).then(r => r.data) }
export function eliminarCategoria(id) { return api.delete(`/rea/categorias/${id}`).then(r => r.data) }
