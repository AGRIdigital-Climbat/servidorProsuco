const API = (window.MOTORIA_CONFIG && window.MOTORIA_CONFIG.apiUrl) || "";

async function request(path, options = {}) {
  return fetch(API + path, options);
}

function json(options = {}) {
  return { "Content-Type": "application/json", ...options };
}

export const apiBase = API;

export function apiUrl(path) {
  return API + (path || "");
}

export const api = {
  health: () => request("/health"),

  tipos: (todos = false) => request(`/tipos-documento${todos ? "?todos=true" : ""}`),
  crearTipo: (data) => request("/tipos-documento", { method: "POST", headers: json(), body: JSON.stringify(data) }),
  editarTipo: (id, data) => request(`/tipos-documento/${id}`, { method: "PUT", headers: json(), body: JSON.stringify(data) }),
  borrarTipo: (id) => request(`/tipos-documento/${id}`, { method: "DELETE" }),

  documentos: () => request("/documentos"),
  documento: (id) => request(`/documentos/${id}`),
  chunks: (id) => request(`/documentos/${id}/chunks`),
  subir: (formData) => request("/documentos", { method: "POST", body: formData }),
  editarDocumento: (id, data) => request(`/documentos/${id}`, { method: "PUT", headers: json(), body: JSON.stringify(data) }),
  borrarDocumento: (id) => request(`/documentos/${id}`, { method: "DELETE" }),
  verificar: (id) => request(`/documentos/${id}/verificar`, { method: "POST" }),
  verificarSistema: (data) => request("/verificacion/general", { method: "POST", headers: json(), body: JSON.stringify(data) }),

  chat: (data) => request("/chat", { method: "POST", headers: json(), body: JSON.stringify(data) }),
  busqueda: (data) => request("/busqueda", { method: "POST", headers: json(), body: JSON.stringify(data) }),
  voz: (formData) => request("/voz", { method: "POST", body: formData }),
};

export async function chatStream(data, onToken, onDone, onError) {
  try {
    const r = await request("/chat", { method: "POST", headers: json(), body: JSON.stringify(data) });
    const ct = r.headers.get("content-type") || "";
    if (!ct.includes("text/event-stream")) {
      const text = await r.text();
      if (text.startsWith("{")) {
        const j = JSON.parse(text);
        if (j.error) { onError(j.error); return; }
      }
      onError("El servidor devolvió una respuesta inesperada. Intente nuevamente.");
      return;
    }

    const reader = r.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buffer += decoder.decode(value, { stream: true });

      const lines = buffer.split("\n");
      buffer = lines.pop();

      for (const line of lines) {
        if (!line.startsWith("data: ")) continue;
        try {
          const evt = JSON.parse(line.slice(6));
          if (evt.type === "token") {
            onToken(evt.content);
          } else if (evt.type === "done") {
            onDone(evt);
          } else if (evt.type === "error") {
            onError(evt.content);
          }
        } catch {}
      }
    }
  } catch (err) {
    onError("Error al conectar con el servidor: " + err.message);
  }
}
