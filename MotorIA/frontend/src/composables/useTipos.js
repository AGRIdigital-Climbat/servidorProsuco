import { ref, computed } from "vue";
import { api } from "../api.js";
import { badgeClass } from "../utils/format.js";

const tipos = ref([]);
const loaded = ref(false);

export function useTipos() {
  async function loadTipos() {
    try {
      const r = await api.tipos();
      const d = await r.json();
      tipos.value = d.tipos || [];
      loaded.value = true;
    } catch {
      loaded.value = true;
    }
  }

  const activos = computed(() => tipos.value.filter((t) => t.activo !== false));

  function claseDe(prioridad) {
    return badgeClass(prioridad);
  }

  function nombreDe(tipoId) {
    const t = tipos.value.find((x) => x.id === tipoId);
    return t ? t.nombre : "";
  }

  return { tipos, activos, loaded, loadTipos, claseDe, nombreDe };
}
