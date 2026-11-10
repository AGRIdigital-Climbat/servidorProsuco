export function escapeHtml(str) {
  if (str === null || str === undefined) return "";
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

export function linkify(texto) {
  return escapeHtml(texto).replace(/\n/g, "<br>");
}

import { marked } from "marked";
import DOMPurify from "dompurify";

const renderer = new marked.Renderer();
const originalLink = renderer.link;
renderer.link = function (href, title, text) {
  const url = href?.href || href || "";
  if (/^https?:\/\//i.test(url)) {
    const t = title ? ` title="${escapeHtml(title)}"` : "";
    return `<a href="${escapeHtml(url)}" target="_blank" rel="noopener noreferrer"${t}>${text}</a>`;
  }
  return text;
};

marked.setOptions({
  renderer,
  breaks: true,
  gfm: true,
});

const CITATION_RE = /\[(\d+(?:\s*,\s*\d+)*)\]/g;

function citationChips(html) {
  return html.replace(CITATION_RE, (_, nums) => {
    return nums
      .split(",")
      .map((n) => `<span class="cita-chip">${n.trim()}</span>`)
      .join(" ");
  });
}

export function renderMd(texto) {
  if (!texto) return "";
  const html = marked.parse(escapeHtml(texto));
  return citationChips(DOMPurify.sanitize(html));
}

export function pct(similitud) {
  return ((similitud ?? 0) * 100).toFixed(1) + "%";
}

export function ahora() {
  return new Date().toLocaleTimeString("es-BO", { hour: "2-digit", minute: "2-digit" });
}

export function fechaCorta(iso) {
  if (!iso) return "";
  const d = new Date(iso);
  return isNaN(d) ? iso : d.toLocaleDateString("es-BO");
}

export function fechaHora(iso) {
  if (!iso) return "";
  const d = new Date(iso);
  return isNaN(d) ? iso : d.toLocaleString("es-BO");
}

/* Color de badge según la prioridad de la jerarquía normativa */
export function badgeLevel(prioridad) {
  if (prioridad === 1) return 1; // Constitución
  if (prioridad >= 2 && prioridad <= 3) return 2; // Tratados y Leyes nacionales
  if (prioridad >= 4 && prioridad <= 5) return 3; // Decretos y Resoluciones Supremas
  if (prioridad >= 6 && prioridad <= 8) return 4; // Normativa departamental
  if (prioridad >= 9 && prioridad <= 13) return 5; // Guías, manuales, reglamentos
  return 6; // Normativa municipal
}

export function badgeClass(prioridad) {
  return `badge badge--lvl${badgeLevel(prioridad)}`;
}

export function copyToClipboard(texto) {
  if (navigator.clipboard) {
    return navigator.clipboard.writeText(texto);
  }
  const ta = document.createElement("textarea");
  ta.value = texto;
  document.body.appendChild(ta);
  ta.select();
  try {
    document.execCommand("copy");
  } finally {
    document.body.removeChild(ta);
  }
  return Promise.resolve();
}
