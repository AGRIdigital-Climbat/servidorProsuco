import api from './axios'

export function listarTiposEvento() { return api.get('/edan/tipo-evento').then(r => r.data) }
