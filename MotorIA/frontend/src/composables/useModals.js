import { reactive } from "vue";
import { api } from "../api.js";

const state = reactive({
  docDetail: { show: false, doc: null, chunks: [], loading: false, error: "" },
  docEdit: { show: false, doc: null, saving: false, error: "", ok: "" },
  tipo: { show: false, tipo: null, saving: false, error: "", ok: "" },
});

export function useModals() {
  async function openDocDetail(docId) {
    const m = state.docDetail;
    m.show = true;
    m.loading = true;
    m.error = "";
    m.doc = null;
    m.chunks = [];
    try {
      const [docRes, chunksRes] = await Promise.all([api.documento(docId), api.chunks(docId)]);
      m.doc = await docRes.json();
      const chunksData = await chunksRes.json();
      m.chunks = chunksData.chunks || [];
    } catch {
      m.error = "Error al cargar el detalle del documento";
    } finally {
      m.loading = false;
    }
  }
  function closeDocDetail() {
    state.docDetail.show = false;
  }

  function openDocEdit(doc) {
    state.docEdit.show = true;
    state.docEdit.doc = doc;
    state.docEdit.error = "";
    state.docEdit.ok = "";
  }
  function closeDocEdit() {
    state.docEdit.show = false;
  }

  function openTipoModal(tipo = null) {
    state.tipo.show = true;
    state.tipo.tipo = tipo;
    state.tipo.error = "";
    state.tipo.ok = "";
  }
  function closeTipoModal() {
    state.tipo.show = false;
  }

  return { state, openDocDetail, closeDocDetail, openDocEdit, closeDocEdit, openTipoModal, closeTipoModal };
}
