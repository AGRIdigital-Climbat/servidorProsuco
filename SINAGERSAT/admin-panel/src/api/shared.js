import api from './axios'

export function listarDepartamentos() { return api.get('/shared/departamentos').then(r => r.data) }
export function crearDepartamento(data) { return api.post('/shared/departamentos', data).then(r => r.data) }
export function actualizarDepartamento(id, data) { return api.put(`/shared/departamentos/${id}`, data).then(r => r.data) }
export function eliminarDepartamento(id) { return api.delete(`/shared/departamentos/${id}`).then(r => r.data) }

export function listarProvincias() { return api.get('/shared/provincias').then(r => r.data) }
export function crearProvincia(data) { return api.post('/shared/provincias', data).then(r => r.data) }
export function actualizarProvincia(id, data) { return api.put(`/shared/provincias/${id}`, data).then(r => r.data) }
export function eliminarProvincia(id) { return api.delete(`/shared/provincias/${id}`).then(r => r.data) }

export function listarMunicipios() { return api.get('/shared/municipios').then(r => r.data) }
export function crearMunicipio(data) { return api.post('/shared/municipios', data).then(r => r.data) }
export function actualizarMunicipio(id, data) { return api.put(`/shared/municipios/${id}`, data).then(r => r.data) }
export function eliminarMunicipio(id) { return api.delete(`/shared/municipios/${id}`).then(r => r.data) }
export function listarMunicipiosGeojson(params) { return api.get('/shared/municipios/geojson', { params }).then(r => r.data) }
