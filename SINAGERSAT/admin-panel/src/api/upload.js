import api from './axios'

export function subirArchivo(formData) {
  return api.post('/upload', formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
  }).then(r => r.data)
}

export function listarArchivos() {
  return api.get('/upload').then(r => r.data)
}
