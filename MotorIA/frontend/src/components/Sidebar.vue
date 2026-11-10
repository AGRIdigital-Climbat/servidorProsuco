<script setup>
import { ref, onMounted } from "vue";
import { api } from "../api.js";
import AppIcon from "./AppIcon.vue";

const TABS = [
  { path: "/chat", icon: "chat", label: "Consultar" },
  { path: "/buscar", icon: "search", label: "Buscar" },
  { path: "/biblioteca", icon: "library", label: "Biblioteca" },
  { path: "/calidad", icon: "bar-chart", label: "Calidad" },
  { path: "/normas", icon: "grid", label: "Tipos de norma" },
];

const status = ref({ text: "Conectando…", online: null });

onMounted(async () => {
  try {
    const r = await api.health();
    await r.json();
    status.value = { text: "En línea", online: true };
  } catch {
    status.value = { text: "Sin conexión", online: false };
  }
});
</script>

<template>
  <aside class="sidebar">
    <div class="sidebar-brand">
      <div class="brand-mark">
        <AppIcon name="scale" :size="24" />
      </div>
      <div>
        <div class="brand-title">MotorIA</div>
        <div class="brand-sub">Asistente Jurídico</div>
      </div>
    </div>

    <nav>
      <RouterLink
        v-for="tab in TABS"
        :key="tab.path"
        :to="tab.path"
        class="nav-btn"
        active-class="active"
      >
        <span class="nav-icon">
          <AppIcon :name="tab.icon" :size="18" />
        </span>
        {{ tab.label }}
      </RouterLink>
    </nav>

    <div class="sidebar-footer">
      <span class="health-pill">
        <span class="health-dot" :class="status.online === null ? '' : status.online ? 'online' : 'offline'"></span>
        {{ status.text }}
      </span>
    </div>
  </aside>
</template>
