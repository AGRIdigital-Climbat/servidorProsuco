# SINAGERSAT Admin Panel

Panel de administración web para SINAGERSAT construido con **Vue 3** + **Vite 5** + **Tailwind CSS 3** + **Pinia** + **Vue Router** + **Axios**.

## Requisitos

- **Node.js >= 18** (probado con v18.19.1)
- **npm >= 9**
- **API BaseData** corriendo en `http://127.0.0.1:8000`

## Instalación

```bash
cd /home/climbat/SINAGERSAT/admin-panel
npm install
```

## Desarrollo

```bash
npm run dev
```

El servidor de desarrollo se levanta en `http://localhost:5173`.  
Las peticiones `/api/*` se redirigen automáticamente a `http://127.0.0.1:8000` (proxy configurado en `vite.config.js`).

Para ver la URL de red (acceder desde otro dispositivo en la misma LAN):

```bash
bash url.sh
```

### Variables de entorno (opcional)

| Variable | Default | Descripción |
|----------|---------|-------------|
| `VITE_API_URL` | `/api` | Base URL de la API (útil si no se usa proxy) |

## Producción

```bash
npm run build
```

Esto genera los archivos estáticos en `dist/`. Luego se sirven con cualquier servidor web:

### Nginx

```nginx
server {
    listen 80;
    server_name admin.sinagersat.local;

    root /path/to/admin-panel/dist;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://127.0.0.1:8000;
    }
}
```

### Caddy

```caddyfile
admin.sinagersat.local {
    root * /path/to/admin-panel/dist
    try_files {path} /index.html
    reverse_proxy /api/* http://127.0.0.1:8000
}
```

## Estructura

```
src/
├── api/           # Axios instance + módulos de endpoints
│   ├── axios.js
│   ├── auth.js
│   ├── usuarios.js
│   ├── shared.js
│   ├── rea.js
│   └── upload.js
├── assets/        # CSS global
├── components/    # DataTable, AppLayout, Sidebar, TopBar
├── router/        # Configuración de rutas con lazy loading
├── stores/        # Pinia stores (auth)
└── views/         # Vistas organizadas por módulo
    ├── auth/          # Login, Register, VerificarIdentidad, CambiarContrasena, MisPermisos
    ├── usuarios/      # CRUD usuarios/roles, Aplicaciones, Acciones, ★ Permisos, Sesiones, Auditoría
    ├── geo/           # Departamentos, Provincias, Municipios
    ├── rea/           # Reportes, Estados, Categorías
    ├── upload/        # Subida de archivos
    └── Dashboard.vue
```

## Vistas principales

| Ruta | Vista | Descripción |
|------|-------|-------------|
| `/` | Dashboard | Inicio |
| `/login` | Login | Autenticación |
| `/usuarios/permisos` | ★ PermisosMatrix | Matriz de permisos por rol × aplicación × CRUD |
| `/usuarios` | UsuariosList | CRUD de usuarios |
| `/rea/reportes` | ReportesList | Listado y filtros de reportes REA |
| `/upload` | UploadView | Subida de imágenes |

## Roles del sistema

| ID | Rol |
|----|------|
| 1 | Superadmin |
| 2 | Admin |
| 3 | VIDECI Regional |
| 4 | UGR Sectores |
| 5 | DGR Gobernación |
| 6 | UGR Municipio |
| 7 | Responsable Local |
