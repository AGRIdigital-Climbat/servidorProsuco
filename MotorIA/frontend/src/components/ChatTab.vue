<script setup>
import { ref, computed, watch, nextTick, onMounted } from "vue";
import { chatStream } from "../api.js";
import { useTipos } from "../composables/useTipos.js";
import { renderMd, ahora, pct, copyToClipboard } from "../utils/format.js";
import AppIcon from "./AppIcon.vue";

const { activos, loadTipos, claseDe } = useTipos();

const SUGERENCIAS = [
  "¿Qué leyes regulan la gestión de residuos sólidos en Bolivia?",
  "Requisitos para una ordenanza municipal de áreas verdes",
  "Derechos de los funcionarios públicos",
  "¿Qué normativa rige la protección civil ante desastres?",
];

const mensajes = ref([]);
const input = ref("");
const tipoFilter = ref("");
const enviando = ref(false);
const ta = ref(null);

const chatScroll = ref(null);

function nuevoMensaje(rol, texto, opts = {}) {
  mensajes.value.push({
    rol,
    texto,
    fuentes: opts.fuentes || [],
    fuentesAbiertas: false,
    tiempo: opts.tiempo || ahora(),
    loading: opts.loading || false,
    transcrito: opts.transcrito || "",
    error: opts.error || false,
    copiado: false,
  });
}

function renderBienvenida() {
  nuevoMensaje(
    "assistant",
    "Hola, soy el asistente jurídico de Bolivia. Puedo responder consultas sobre la Constitución, leyes, decretos, resoluciones, ordenanzas municipales y demás normativa cargada en la biblioteca. Las respuestas se basan exclusivamente en los documentos que contiene el sistema.",
    { tiempo: "" },
  );
}

onMounted(async () => {
  renderBienvenida();
  await loadTipos();
});

function usarSugerencia(s) {
  input.value = s;
  enviar();
}

function ajustarAlto() {
  if (!ta.value) return;
  ta.value.style.height = "auto";
  ta.value.style.height = Math.min(ta.value.scrollHeight, 140) + "px";
}

function onKeydown(e) {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    enviar();
  }
}

async function enviar() {
  const pregunta = input.value.trim();
  if (!pregunta || enviando.value) return;

  enviando.value = true;
  if (mensajes.value.length === 1) mensajes.value = [];

  nuevoMensaje("user", pregunta);
  input.value = "";
  ajustarAlto();
  scrollAbajo();

  const idx = mensajes.value.push({
    rol: "assistant",
    texto: "",
    fuentes: [],
    loading: true,
    tiempo: ahora(),
    error: false,
  }) - 1;

  const body = { pregunta, limite: 10 };
  if (tipoFilter.value) body.tipo_id = parseInt(tipoFilter.value);

  await chatStream(
    body,
    (token) => {
      mensajes.value[idx].texto += token;
      if (token) mensajes.value[idx].loading = false;
      scrollAbajo();
    },
    (done) => {
      mensajes.value[idx].fuentes = done.fuentes || [];
      mensajes.value[idx].loading = false;
      enviando.value = false;
      scrollAbajo();
    },
    (err) => {
      mensajes.value[idx] = {
        rol: "assistant",
        texto: err,
        fuentes: [],
        loading: false,
        tiempo: ahora(),
        error: true,
      };
      enviando.value = false;
    },
  );
}

async function copiar(m) {
  try {
    await copyToClipboard(m.texto);
    m.copiado = true;
    setTimeout(() => (m.copiado = false), 1600);
  } catch {}
}

function scrollAbajo() {
  nextTick(() => {
    if (chatScroll.value) chatScroll.value.scrollTop = chatScroll.value.scrollHeight;
  });
}

watch(mensajes, scrollAbajo, { deep: true });

const muestraSugerencias = computed(() => mensajes.value.length === 1);
</script>

<template>
  <div class="chat-wrap">
    <header class="chat-header">
      <div class="chat-title">
        <div class="chat-avatar">
          <AppIcon name="scale" :size="20" />
        </div>
        <div>
          <h3>Asistente Jurídico</h3>
          <span class="sub">Responde citando normativa boliviana · {{ activos.length }} tipos de norma indexados</span>
        </div>
      </div>
      <div class="chat-filter">
        <label for="tipoFilter">Norma:</label>
        <select id="tipoFilter" v-model="tipoFilter" class="form-control filter-select">
          <option value="">Todas las normas</option>
          <option v-for="t in activos" :key="t.id" :value="t.id">{{ t.prioridad }} · {{ t.nombre }}</option>
        </select>
      </div>
    </header>

    <div class="chat-scroll" ref="chatScroll">
      <div class="chat-messages">
        <TransitionGroup name="msg">
          <div
            v-for="(m, i) in mensajes"
            :key="m.tiempo + i"
            class="message"
            :class="[m.rol, { 'message--error': m.error }]"
          >
            <div v-if="m.rol === 'assistant'" class="msg-avatar">
              <AppIcon name="scale" :size="16" />
            </div>

            <div class="msg-body">
              <div class="msg-meta">
                <span class="msg-author">{{ m.rol === "user" ? "Usted" : "Asistente Jurídico" }}</span>
                <span v-if="m.tiempo" class="msg-time">{{ m.tiempo }}</span>
              </div>

              <div v-if="m.loading" class="typing">
                <span class="typing-dot"></span><span class="typing-dot"></span><span class="typing-dot"></span>
                <span class="typing-label">buscando en la normativa…</span>
              </div>

              <template v-else>
                <div class="msg-text" v-html="renderMd(m.texto)"></div>

                <p v-if="m.transcrito" class="transcript">
                  <em>Transcripción: {{ m.transcrito }}</em>
                </p>

                <div v-if="m.fuentes.length" class="fuentes">
                  <button
                    class="fuentes-toggle"
                    :class="{ open: m.fuentesAbiertas }"
                    @click="m.fuentesAbiertas = !m.fuentesAbiertas"
                  >
                    <span class="ft-left">
                      <span class="app-icon">
                        <AppIcon name="layers" :size="14" />
                      </span>
                      Fuentes ({{ m.fuentes.length }} documentos)
                    </span>
                    <span class="chevron" :class="{ open: m.fuentesAbiertas }">
                      <AppIcon name="chevron-down" :size="13" />
                    </span>
                  </button>

                  <Transition name="fuentes">
                    <div v-if="m.fuentesAbiertas" class="fuentes-list">
                      <div v-for="(f, fi) in m.fuentes" :key="fi" class="fuente-item">
                        <span class="fuente-num">{{ fi + 1 }}</span>
                        <div class="fuente-info">
                          <span class="badge" :class="claseDe(f.prioridad ?? f.tipo_id)">{{ f.tipo }}</span>
                          <strong>{{ f.titulo }}</strong>
                          <span v-if="f.numerals && f.numerals.length" class="fuente-numeral">
                            ({{ f.numerals.join(", ") }}<span v-if="f.numerals_truncados">…</span>)
                          </span>
                          <span class="fuente-similitud">Relevancia {{ pct(f.similitud) }}</span>
                        </div>
                      </div>
                    </div>
                  </Transition>
                </div>
              </template>

              <button
                v-if="m.rol === 'assistant' && m.texto && !m.loading"
                class="copy-btn"
                title="Copiar respuesta"
                @click="copiar(m)"
              >
                <span v-if="m.copiado" class="app-icon">
                  <AppIcon name="check" :size="13" />
                </span>
                <span v-else class="app-icon">
                  <AppIcon name="copy" :size="13" />
                </span>
                {{ m.copiado ? "Copiado" : "Copiar" }}
              </button>
            </div>

            <div v-if="m.rol === 'user'" class="msg-avatar user">U</div>
          </div>
        </TransitionGroup>
      </div>
    </div>

    <div class="chat-footer">
      <Transition name="fuentes">
        <div v-if="muestraSugerencias" class="suggestions">
          <span class="suggestions-label">¿Qué deseas consultar?</span>
          <button v-for="s in SUGERENCIAS" :key="s" class="suggestion-chip" @click="usarSugerencia(s)">
            {{ s }}
          </button>
        </div>
      </Transition>

      <div class="chat-input-area">
        <div class="input-box">
          <textarea
            ref="ta"
            v-model="input"
            rows="1"
            placeholder="Escriba su consulta jurídica…"
            @input="ajustarAlto"
            @keydown="onKeydown"
          ></textarea>
        </div>

        <button class="send-btn" :disabled="enviando || !input.trim()" title="Enviar" @click="enviar">
          <span v-if="enviando" class="spin"></span>
          <span v-else class="send-icon">
            <AppIcon name="send" :size="18" />
          </span>
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.chat-wrap {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  max-width: 980px;
  width: 100%;
  margin: 0 auto;
}

.chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 18px 8px 14px;
  border-bottom: 1px solid var(--border-soft);
  flex-wrap: wrap;
}

.chat-title {
  display: flex;
  align-items: center;
  gap: 12px;
}

.chat-avatar {
  width: 42px;
  height: 42px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  font-size: 20px;
  background: linear-gradient(135deg, var(--accent-soft), var(--gold-soft));
  border: 1px solid var(--border);
}

.chat-title h3 {
  font-size: 16.5px;
  font-weight: 700;
}

.chat-title .sub {
  font-size: 12px;
  color: var(--text-muted);
}

.chat-filter {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12.5px;
  color: var(--text-muted);
}

.filter-select {
  width: auto;
  min-width: 180px;
  padding: 7px 30px 7px 10px;
  font-size: 13px;
}

.chat-scroll {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  padding: 20px 8px;
}

.chat-messages {
  display: flex;
  flex-direction: column;
  gap: 18px;
}

.message {
  display: flex;
  gap: 12px;
  max-width: 92%;
}

.message.user {
  align-self: flex-end;
  flex-direction: row-reverse;
}

.msg-avatar {
  flex-shrink: 0;
  width: 34px;
  height: 34px;
  border-radius: 10px;
  display: grid;
  place-items: center;
  font-size: 16px;
  background: var(--surface-2);
  border: 1px solid var(--border);
}

.msg-avatar.user {
  background: linear-gradient(135deg, var(--accent), var(--accent-strong));
  border: none;
  color: #fff;
  font-weight: 700;
  font-size: 13px;
}

.msg-body {
  min-width: 0;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 4px 16px 16px 16px;
  padding: 12px 16px;
}

.message.user .msg-body {
  background: linear-gradient(135deg, rgba(99, 102, 241, 0.13), rgba(139, 92, 246, 0.07));
  border-color: rgba(99, 102, 241, 0.26);
  border-radius: 16px 4px 16px 16px;
}

.message--error .msg-body {
  border-color: rgba(248, 113, 113, 0.4);
}

.msg-meta {
  display: flex;
  align-items: baseline;
  gap: 10px;
  margin-bottom: 6px;
}

.msg-author {
  font-size: 12.5px;
  font-weight: 700;
  color: var(--accent);
}

.message.user .msg-author {
  color: var(--gold);
}

.msg-time {
  font-size: 11px;
  color: var(--text-dim);
}

.msg-text {
  font-size: 14.5px;
  line-height: 1.65;
  word-break: break-word;
}

.msg-text:deep(p) {
  margin: 8px 0;
}

.msg-text:deep(strong) {
  font-weight: 700;
}

.msg-text:deep(em) {
  font-style: italic;
}

.msg-text:deep(ul),
.msg-text:deep(ol) {
  margin: 8px 0;
  padding-left: 24px;
}

.msg-text:deep(li) {
  margin: 4px 0;
}

.msg-text:deep(h1),
.msg-text:deep(h2),
.msg-text:deep(h3),
.msg-text:deep(h4) {
  margin: 12px 0 6px;
  font-weight: 700;
}

.msg-text:deep(h1) { font-size: 18px; }
.msg-text:deep(h2) { font-size: 16px; }
.msg-text:deep(h3) { font-size: 15px; }

.msg-text:deep(code) {
  background: var(--bg-secondary, #f1f5f9);
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 13px;
  font-family: monospace;
}

.msg-text:deep(blockquote) {
  border-left: 3px solid var(--accent);
  margin: 8px 0;
  padding: 6px 12px;
  color: var(--text-muted);
  background: var(--bg-secondary, #f8fafc);
  border-radius: 0 6px 6px 0;
}

.msg-text:deep(a) {
  color: var(--accent);
  text-decoration: underline;
}

.cita-chip {
  display: inline-block;
  background: linear-gradient(135deg, #c9a84c22, #c9a84c33);
  border: 1px solid var(--gold, #c9a84c);
  color: var(--gold-dark, #8b6914);
  font-size: 11px;
  font-weight: 700;
  padding: 1px 7px;
  border-radius: 10px;
  margin: 0 2px;
  vertical-align: baseline;
  line-height: 1.6;
}

.transcript {
  margin-top: 8px;
  font-size: 12px;
  color: var(--text-muted);
}

/* Fuentes */
.fuentes {
  margin-top: 12px;
  border-top: 1px dashed var(--border);
  padding-top: 10px;
}

.fuentes-toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text-muted);
  background: var(--bg-soft);
  border: 1px solid var(--border-soft);
  border-radius: 8px;
  padding: 7px 12px;
}

.ft-left {
  display: inline-flex;
  align-items: center;
  gap: 8px;
}

.fuentes-toggle:hover {
  color: var(--text);
  border-color: var(--border);
}

.chevron {
  display: grid;
  place-items: center;
  transition: transform 0.2s ease;
}

.chevron.open {
  transform: rotate(180deg);
}

.fuentes-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
}

.fuente-item {
  display: flex;
  gap: 10px;
  align-items: flex-start;
  background: var(--bg-soft);
  border: 1px solid var(--border-soft);
  border-radius: 10px;
  padding: 10px 12px;
}

.fuente-num {
  flex-shrink: 0;
  width: 22px;
  height: 22px;
  border-radius: 50%;
  background: var(--accent-soft);
  color: var(--accent);
  font-size: 11.5px;
  font-weight: 700;
  display: grid;
  place-items: center;
}

.fuente-info {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
  font-size: 13px;
  min-width: 0;
}

.fuente-info strong {
  color: var(--text);
}

.fuente-numeral {
  color: var(--text-dim);
  font-size: 12px;
}

.fuente-similitud {
  margin-left: auto;
  font-size: 11px;
  color: var(--text-dim);
  font-family: var(--mono);
}

.copy-btn {
  margin-top: 10px;
  font-size: 11.5px;
  color: var(--text-dim);
  padding: 4px 10px;
  border-radius: 7px;
  background: var(--bg-soft);
  border: 1px solid var(--border-soft);
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.copy-btn:hover {
  color: var(--accent);
  border-color: rgba(99, 102, 241, 0.4);
}

/* Typing */
.typing {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 4px 0;
}

.typing-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--accent);
  animation: bounce 1.2s infinite;
}

.typing-dot:nth-child(2) {
  animation-delay: 0.15s;
}

.typing-dot:nth-child(3) {
  animation-delay: 0.3s;
}

@keyframes bounce {
  0%, 60%, 100% {
    transform: translateY(0);
    opacity: 0.5;
  }
  30% {
    transform: translateY(-5px);
    opacity: 1;
  }
}

.typing-label {
  margin-left: 10px;
  font-size: 12px;
  color: var(--text-dim);
}

/* Footer / input */
.chat-footer {
  padding: 10px 8px 18px;
}

.suggestions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
  margin-bottom: 12px;
}

.suggestions-label {
  font-size: 12px;
  color: var(--text-dim);
  width: 100%;
  margin-bottom: 2px;
}

.suggestion-chip {
  font-size: 12.5px;
  color: var(--text-muted);
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 999px;
  padding: 7px 14px;
  transition: all 0.15s;
}

.suggestion-chip:hover {
  color: var(--accent);
  border-color: rgba(99, 102, 241, 0.5);
  background: var(--accent-soft);
}

.chat-input-area {
  display: flex;
  align-items: flex-end;
  gap: 10px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 16px;
  padding: 10px;
  transition: border-color 0.15s, box-shadow 0.15s;
}

.chat-input-area:focus-within {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.15);
}

.input-box {
  position: relative;
  flex: 1;
}

.input-box textarea {
  width: 100%;
  border: none;
  outline: none;
  background: transparent;
  resize: none;
  font-size: 14.5px;
  line-height: 1.5;
  max-height: 140px;
  padding: 8px 4px;
}

.send-btn {
  flex-shrink: 0;
  width: 40px;
  height: 40px;
  border-radius: 12px;
  background: linear-gradient(135deg, var(--accent), var(--accent-strong));
  color: #fff;
  font-size: 15px;
  display: grid;
  place-items: center;
  box-shadow: 0 6px 16px rgba(99, 102, 241, 0.35);
}

.send-icon {
  display: grid;
  place-items: center;
}

.send-icon .app-icon {
  display: block;
}

.send-btn:disabled {
  opacity: 0.45;
  box-shadow: none;
}

/* Transiciones de mensajes */
.msg-enter-active {
  transition: all 0.3s ease;
}

.msg-enter-from {
  opacity: 0;
  transform: translateY(14px) scale(0.98);
}

.fuentes-enter-active,
.fuentes-leave-active {
  transition: all 0.18s ease;
}

.fuentes-enter-from,
.fuentes-leave-to {
  opacity: 0;
  transform: translateY(-4px);
}
</style>
