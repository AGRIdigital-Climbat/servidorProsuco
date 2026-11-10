import { createRouter, createWebHashHistory } from "vue-router";
import ChatTab from "../components/ChatTab.vue";
import BusquedaTab from "../components/BusquedaTab.vue";
import DocumentosTab from "../components/DocumentosTab.vue";
import CalidadesTab from "../components/CalidadesTab.vue";
import TiposTab from "../components/TiposTab.vue";

const routes = [
  { path: "/", redirect: "/chat" },
  { path: "/chat", name: "chat", component: ChatTab },
  { path: "/buscar", name: "busqueda", component: BusquedaTab },
  { path: "/biblioteca", name: "documentos", component: DocumentosTab },
  { path: "/calidad", name: "calidad", component: CalidadesTab },
  { path: "/normas", name: "tipos", component: TiposTab },
];

export default createRouter({
  history: createWebHashHistory(),
  routes,
});
