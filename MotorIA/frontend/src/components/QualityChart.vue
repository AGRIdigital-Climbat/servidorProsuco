<script setup>
import { computed } from "vue";

const props = defineProps({
  verificacion: { type: Object, required: true },
  titulo: { type: String, default: "Índice de calidad" },
  indice: { type: Number, default: null },
});

const R = 42;
const C = 2 * Math.PI * R;

function colorDe(val) {
  if (val >= 0.8) return "var(--success)";
  if (val >= 0.5) return "var(--warning)";
  return "var(--danger)";
}

const valorDonut = computed(() => props.indice ?? (props.verificacion.precision_at_5 ?? 0));

const indice = computed(() => Math.round(valorDonut.value * 100));

const indiceColor = computed(() => colorDe(valorDonut.value));

const totalQueries = computed(() => props.verificacion.total_queries ?? 0);

const barras = computed(() => {
  const v = props.verificacion;
  return [
    {
      etiqueta: "Primer resultado",
      desc: "De cada 100 preguntas de prueba, en cuántas este documento fue la coincidencia N.º 1.",
      valor: v.precision_at_1 ?? 0,
    },
    {
      etiqueta: "Dentro de los 3 mejores",
      desc: "En cuántas preguntas este documento quedó entre las 3 mejores coincidencias.",
      valor: v.precision_at_3 ?? 0,
    },
    {
      etiqueta: "Dentro de los 5 mejores",
      desc: "En cuántas preguntas este documento quedó entre las 5 mejores (cobertura general).",
      valor: v.precision_at_5 ?? 0,
    },
    {
      etiqueta: "Similitud del contenido",
      desc: "Qué tan parecidos son los fragmentos encontrados a las preguntas (0 = nada, 1 = muy parecido).",
      valor: v.average_similarity ?? 0,
    },
  ];
});
</script>

<template>
  <div class="quality">
    <div class="quality-summary">
      <div class="donut" :style="{ '--color': indiceColor }">
        <svg viewBox="0 0 100 100">
          <circle class="donut-bg" cx="50" cy="50" :r="R" />
          <circle
            class="donut-fg"
            cx="50"
            cy="50"
            :r="R"
            :stroke-dasharray="`${(indice / 100) * C} ${C}`"
          />
        </svg>
        <div class="donut-value">{{ indice }}%</div>
      </div>
      <div class="summary-text">
        <strong>{{ titulo }}</strong>
        <p>
          Resultado global de las pruebas de búsqueda.
          A mayor porcentaje, mejor responde el sistema.
        </p>
        <span v-if="totalQueries" class="summary-note">
          Basado en {{ totalQueries }} consultas de prueba
        </span>
      </div>
    </div>

    <div class="bars">
      <div v-for="b in barras" :key="b.etiqueta" class="bar-item">
        <div class="bar-head">
          <span class="bar-label">{{ b.etiqueta }}</span>
          <span class="bar-value" :style="{ color: colorDe(b.valor) }">
            {{ Math.round(b.valor * 100) }}%
          </span>
        </div>
        <div class="bar-track">
          <div class="bar-fill" :style="{ width: Math.round(b.valor * 100) + '%', background: colorDe(b.valor) }"></div>
        </div>
        <p class="bar-desc">{{ b.desc }}</p>
      </div>
    </div>
  </div>
</template>

<style scoped>
.quality {
  display: flex;
  flex-direction: column;
  gap: 18px;
}

.quality-summary {
  display: flex;
  align-items: center;
  gap: 18px;
}

.donut {
  position: relative;
  width: 92px;
  height: 92px;
  flex-shrink: 0;
}

.donut svg {
  width: 100%;
  height: 100%;
  transform: rotate(-90deg);
}

.donut-bg {
  fill: none;
  stroke: var(--bg-soft);
  stroke-width: 10;
}

.donut-fg {
  fill: none;
  stroke: var(--color, var(--accent));
  stroke-width: 10;
  stroke-linecap: round;
  transition: stroke-dasharray 0.8s ease;
}

.donut-value {
  position: absolute;
  inset: 0;
  display: grid;
  place-items: center;
  font-size: 19px;
  font-weight: 700;
  font-family: var(--mono);
  color: var(--text);
}

.summary-text strong {
  display: block;
  font-size: 14px;
  color: var(--text);
}

.summary-text p {
  font-size: 12.5px;
  color: var(--text-muted);
  line-height: 1.5;
  margin-top: 4px;
}

.summary-note {
  display: inline-block;
  margin-top: 8px;
  font-size: 11px;
  font-weight: 600;
  color: var(--accent-strong);
  background: var(--accent-soft);
  padding: 3px 10px;
  border-radius: 999px;
}

.bars {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.bar-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 5px;
}

.bar-label {
  font-size: 12.5px;
  font-weight: 600;
  color: var(--text);
}

.bar-value {
  font-size: 12.5px;
  font-weight: 700;
  font-family: var(--mono);
}

.bar-track {
  height: 8px;
  border-radius: 999px;
  background: var(--bg-soft);
  overflow: hidden;
}

.bar-fill {
  height: 100%;
  border-radius: 999px;
  transition: width 0.7s ease;
}

.bar-desc {
  font-size: 11.5px;
  color: var(--text-dim);
  line-height: 1.45;
  margin-top: 5px;
}
</style>
